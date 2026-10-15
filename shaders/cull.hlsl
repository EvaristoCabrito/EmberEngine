// GPU-driven visibility for the mesh scene, before any triangle is drawn:
//   CSInstancesEarly / CSInstancesLate
//                 one thread per instance. Two-phase occlusion culling: the early pass draws what was
//                 visible last frame; a depth pyramid is built from it; the late pass tests every instance
//                 against the pyramid, records the verdict for the next frame, and draws what is visible
//                 now but was not drawn early. Drawn instances put the roots of their mesh's per-level BVHs
//                 into queue 0.
//   CSPrepare     one thread: indirect arguments for the next traversal pass; clears its output queue.
//   CSTraverse    one thread per queue item (instance, BVH node): a node whose screen error is over the
//                 threshold is opened: an inner node queues its children, a leaf picks the clusters of its
//                 group that are exactly detailed enough (cluster LOD rule, MeshFormat.h) into the list.
//   CSPrepareDraw one thread: task-shader arguments for this pass's part of the cluster list (the late
//                 pass appends after the early one).
// Queues hold uint2 (instance, node); the cluster list holds uint2 (instance, cluster).
#include "scene.hlsli"

struct Push {
  uint64_t frame;
  uint pass;  // traversal: reads queue (pass & 1), writes queue (pass & 1) ^ 1; CSPrepareDraw: 0 early, 1 late
};
[[vk::push_constant]] Push push;

static const uint kGroupSize = 64;

// Appends `count` items to a queue; returns the first slot, or ~0 when the queue is full (counted).
uint reserveQueue(MeshFrame f, uint queue, uint count) {
  uint first;
  work(f).InterlockedAdd(queue * 4, count, first);
  if (first + count > f.maxQueue) {
    work(f).InterlockedAdd(kCounterQueueOverflow * 4, count);
    return ~0u;
  }
  return first;
}

void emitCluster(MeshFrame f, uint instance, uint cluster) {
  uint slot;
  work(f).InterlockedAdd(kCounterClusters * 4, 1, slot);
  if (slot >= f.maxClusters) {
    work(f).InterlockedAdd(kCounterClusterOverflow * 4, 1);
    return;
  }
  work(f).Store2(clusterListOffset(f) + slot * 8, uint2(instance, cluster));
}

void pushRoots(MeshFrame f, uint instance, MeshGpu mesh) {
  const uint first = reserveQueue(f, 0, mesh.levelCount);
  if (first == ~0u) return;
  for (uint level = 0; level < mesh.levelCount; ++level)
    work(f).Store2(queueOffset(f, 0) + (first + level) * 8, uint2(instance, level));
}

void cullInstance(uint id, bool late) {
  const MeshFrame f = loadFrame(push.frame);
  if (id >= f.instanceCount) return;
  const InstanceGpu inst = loadInstance(f, id);
  if (inst.mesh >= f.meshCount) return;
  const MeshGpu mesh = loadMesh(f, inst.mesh);
  if (mesh.ready == 0) return;
  // Fog of war (R7): a piece standing on never-seen ground is neither drawn nor casts a shadow. Like a card, a
  // piece is seen whole by its anchor's hex (no house cut in half at the boundary).
  if (neverSeen(f, instanceAnchor(inst))) return;
  RWByteAddressBuffer visibility = gBuffers[NonUniformResourceIndex(f.instanceVisibility)];
  const bool wasVisible = visibility.Load(id * 4) != 0;
  const float3 center = transformPoint(inst, mesh.sphere.xyz);
  const float radius = mesh.sphere.w * inst.scale;
  const bool inFrustum = sphereInFrustum(f, center, radius);
  if (f.shadowPass != 0) {
    // From the sun: everything in its frustum casts (no occlusion history for this view).
    if (!late && inFrustum) {
      work(f).InterlockedAdd(kCounterInstances * 4, 1);
      pushRoots(f, id, mesh);
    }
    return;
  }
  if (!late) {
    if (inFrustum && wasVisible) {
      work(f).InterlockedAdd(kCounterInstances * 4, 1);
      pushRoots(f, id, mesh);
    }
    return;
  }
  const bool visible = inFrustum && sphereVisibleHiZ(f, center, radius);
  visibility.Store(id * 4, visible ? 1u : 0u);
  if (inFrustum && !visible) work(f).InterlockedAdd(kCounterOccludedInstances * 4, 1);
  if (visible && !wasVisible) {
    work(f).InterlockedAdd(kCounterInstances * 4, 1);
    pushRoots(f, id, mesh);
  }
}

[numthreads(kGroupSize, 1, 1)]
void CSInstancesEarly(uint id : SV_DispatchThreadID) { cullInstance(id, false); }

[numthreads(kGroupSize, 1, 1)]
void CSInstancesLate(uint id : SV_DispatchThreadID) { cullInstance(id, true); }

[numthreads(1, 1, 1)]
void CSPrepare() {
  const MeshFrame f = loadFrame(push.frame);
  const uint input = push.pass & 1;
  const uint count = min(work(f).Load(input * 4), f.maxQueue);
  work(f).Store3(kTraverseArgs, uint3((count + kGroupSize - 1) / kGroupSize, 1, 1));
  work(f).Store((input ^ 1) * 4, 0);
}

[numthreads(kGroupSize, 1, 1)]
void CSTraverse(uint id : SV_DispatchThreadID) {
  const MeshFrame f = loadFrame(push.frame);
  const uint input = push.pass & 1, output = input ^ 1;
  if (id >= min(work(f).Load(input * 4), f.maxQueue)) return;
  const uint2 item = work(f).Load2(queueOffset(f, input) + id * 8);
  const InstanceGpu inst = loadInstance(f, item.x);
  const MeshGpu mesh = loadMesh(f, inst.mesh);
  const MeshNode node = loadNode(mesh, item.y);

  // Everything under this node is already finer than needed: a coarser level draws it.
  if (screenError(f, inst, node.center, node.radius, node.error) <= 1.0) return;

  if (node.group < 0) {
    const uint first = reserveQueue(f, output, node.childCount);
    if (first == ~0u) return;
    for (uint c = 0; c < node.childCount; ++c)
      work(f).Store2(queueOffset(f, output) + (first + c) * 8, uint2(item.x, node.childOffset + c));
    return;
  }

  // Leaf: this group's simplified version is too coarse, so its own clusters are candidates; each is
  // drawn unless the more detailed group it came from is still needed instead.
  const MeshGroup group = loadGroup(mesh, uint(node.group));
  for (uint c = 0; c < group.clusterCount; ++c) {
    const uint index = group.firstCluster + c;
    const MeshCluster cluster = loadCluster(mesh, index);
    bool draw = cluster.refinedGroup < 0;
    if (!draw) {
      const MeshGroup refined = loadGroup(mesh, uint(cluster.refinedGroup));
      draw = screenError(f, inst, refined.center, refined.radius, refined.error) <= 1.0;
    }
    if (draw) emitCluster(f, item.x, index);
  }
}

[numthreads(1, 1, 1)]
void CSPrepareDraw() {
  const MeshFrame f = loadFrame(push.frame);
  const uint total = min(work(f).Load(kCounterClusters * 4), f.maxClusters);
  uint count = total;
  if (push.pass == 0) work(f).Store(kCounterEarlyClusters * 4, total);
  else count = total - work(f).Load(kCounterEarlyClusters * 4) + min(work(f).Load(kCounterRetest * 4), f.maxClusters);
  const uint groups = (count + kGroupSize - 1) / kGroupSize;
  // Task workgroups: x stays within the minimum every GPU supports (65535); y carries the rest.
  const uint x = min(groups, 65535u);
  work(f).Store3(kDrawArgs, uint3(x, x == 0 ? 0 : (groups + x - 1) / x, 1));
}
