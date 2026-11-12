from pathlib import Path
p = Path(r'C:\WastelandFarming\app.js')
s = p.read_text(encoding='utf-8')
def change(old, new):
 global s
 assert s.count(old) == 1, old
 s = s.replace(old,new,1)
change("{ title: 'Noites de Ossos: abata 4 caveiras ou ladrões',\n    progress: () => ({ have: state.goalKills || 0, need: 4, text: `${Math.min(4, state.goalKills || 0)} / 4 abatidos` }),", "{ title: 'Destrua um covil: na aba Heróis, ofereça uma recompensa para destruir um covil.',\n    progress: () => { const destroyed = (state.lairs || []).some(lair => lair.destroyed); return { have: destroyed ? 1 : 0, need: 1, text: destroyed ? 'Covil destruído' : 'Nenhum covil destruído' }; },")
change("'Agora os mortos do vale querem a parte deles. Abata 4 caveiras ou ladrões.'", "'Destrua um covil: na aba Heróis, ofereça uma recompensa para destruir um covil.'")
change("$('#objective-step').textContent = `${step + 1} de ${SCENARIO_GOALS.length}`;", "$('#objective-step').textContent = 'atual';")
change('const panelScroll = {};', 'const panelScroll = {};\nconst openBuildCategories = new Set();')
change("  $$('.build-card', host).forEach(button => button.addEventListener('click', () => chooseBuild(button.dataset.build)));", "  $$('[data-build-category]', host).forEach(section => section.addEventListener('toggle', () => {\n    if (section.open) openBuildCategories.add(section.dataset.buildCategory);\n    else openBuildCategories.delete(section.dataset.buildCategory);\n  }));\n  $$('.build-card', host).forEach(button => button.addEventListener('click', () => chooseBuild(button.dataset.build)));")
change('${BUILD_CATEGORIES.map(category => `<section class="build-section" aria-label="${category.name}"><h4 class="build-section-title">${category.name}</h4><div class="build-grid">${BUILDINGS.filter(item => buildCategoryOf(item.id) === category.id).map(buildCardHTML).join(\'\')}</div></section>`).join(\'\')}', '${[...BUILD_CATEGORIES].sort((a, b) => a.name.localeCompare(b.name, \'pt-BR\')).map(category => `<details class="build-section" data-build-category="${category.id}" ${openBuildCategories.has(category.id) ? \'open\' : \'\'}><summary class="build-section-title">${category.name}</summary><div class="build-grid">${BUILDINGS.filter(item => buildCategoryOf(item.id) === category.id).sort((a, b) => a.name.localeCompare(b.name, \'pt-BR\')).map(buildCardHTML).join(\'\')}</div></details>`).join(\'\')}')
p.write_text(s, encoding='utf-8', newline='')

