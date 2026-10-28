// Show the shared grab hand during right-button drags on every Engine2 screen.
(() => {
  const root = document.documentElement;
  const stopGrab = () => root.classList.remove('engine2-grabbing');

  document.addEventListener('pointerdown', event => {
    if (event.button === 2) root.classList.add('engine2-grabbing');
  }, true);
  document.addEventListener('pointerup', stopGrab, true);
  document.addEventListener('pointercancel', stopGrab, true);
  window.addEventListener('blur', stopGrab);
})();
