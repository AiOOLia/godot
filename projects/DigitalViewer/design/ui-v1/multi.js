/* ==========================================================================
   DigitalViewer — Multi-Viewport prototype logic
   递归分割树：分割只替换目标视口；不设数量/尺寸上限
   ========================================================================== */
(() => {
  'use strict';

  const $ = (sel, root = document) => root.querySelector(sel);
  const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));
  const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
  const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));

  const TYPE = {
    globe: { icon: 'globe', cls: 't-globe', label: '椭球' },
    image: { icon: 'image', cls: 't-image', label: '影像' },
    terrain: { icon: 'mountain', cls: 't-terrain', label: 'DEM' },
    tiles: { icon: 'box', cls: 't-tiles', label: '3D TILES' },
    points: { icon: 'points', cls: 't-points', label: '点云' },
    osgb: { icon: 'archive', cls: 't-osgb', label: 'OSGB' },
    gltf: { icon: 'box', cls: 't-gltf', label: 'glTF' },
    script: { icon: 'terminal', cls: 't-script', label: '脚本' }
  };
  const DEFAULT_SETTINGS = {
    projection: '透视', fov: 60, render: 'textured',
    grid: true, terrain: true, exag: 1.5, bg: 'space', light: 'default'
  };
  const TREE_GROUPS = [
    { id: 'base', label: '底图', icon: 'image', types: ['globe', 'image'] },
    { id: 'terrain', label: '地形', icon: 'mountain', types: ['terrain'] },
    { id: '3d', label: '三维数据', icon: 'box', types: ['tiles', 'points', 'osgb', 'gltf'] },
    { id: 'script', label: '脚本', icon: 'terminal', types: ['script'] }
  ];
  const mkLayer = (key, type, name, sub, extra = {}) => ({ key, type, name, sub, visible: true, opacity: 1, ...extra });
  const globeLayer = () => mkLayer('globe', 'globe', 'WGS84 椭球', '椭球 · 就绪', {
    meta: { format: 'WGS84', semiMajor: '6,378,137 m', flattening: '1/298.257223563', datum: 'WGS84' }
  });

  const SEEDS = {
    1: {
      mode: '轨道相机', camera: { scale: 1, rot: 0 },
      coord: { lon: 121.4737, lat: 31.2304, h: 8.42 },
      settings: {},
      layers: [
        mkLayer('imagery', 'image', '全球影像底图', 'XYZ · 124 瓦片', {
          meta: { format: 'XYZ', crs: 'EPSG:3857', zoom: '0–19', tiles: '124 / 186', attribution: '© 示例影像' },
          brightness: 1, contrast: 1
        }),
        mkLayer('terrain', 'terrain', '区域高程 DEM', 'COG · 加载 82%', {
          state: 'loading', progress: 82, terrainActive: true, exag: 1.5,
          meta: { format: 'COG', resolution: '30 m', elevation: '−12 – 428 m', nodata: '−9999', crs: 'EPSG:4326' }
        }),
        mkLayer('buildings', 'tiles', '城市建筑 3D Tiles', '就绪 · 238 MB', {
          meta: { format: '3D Tiles 1.1', tiles: '238 / 412', geometricError: '0.5 m', sse: '16 px', memory: '238 MB' },
          outline: false
        })
      ],
      selected: 'buildings'
    },
    2: {
      mode: '飞行相机', camera: { scale: 1.16, rot: 12 },
      coord: { lon: 121.5057, lat: 31.2397, h: 1.8 },
      settings: { fov: 45, exag: 2.0, bg: 'warm' },
      layers: [
        mkLayer('imagery', 'image', '全球影像底图', '共享数据 · 独立显示', {
          meta: { format: 'XYZ', crs: 'EPSG:3857', zoom: '0–19', tiles: '124 / 186', attribution: '© 示例影像' },
          brightness: 1, contrast: 1
        }),
        mkLayer('points', 'points', '测绘点云', 'PNTS · 解析 72%', {
          state: 'loading', progress: 72,
          meta: { format: 'PNTS', points: '12.4 M', spacing: '0.15 m', color: 'RGB', budget: '180 MB' },
          pointSize: 2.4, colorMode: 'RGB'
        })
      ],
      selected: 'points'
    },
    3: {
      mode: '轨道相机', camera: { scale: 0.92, rot: -8 },
      coord: { lon: 121.4702, lat: 31.2286, h: 0.42 },
      settings: { render: 'unlit', grid: false, exag: 1.0, bg: 'flat' },
      layers: [
        mkLayer('osgb', 'osgb', '园区 OSGB', 'PagedLOD · 已暂停', {
          state: 'paused',
          meta: { format: 'OSGB / PagedLOD', lod: '4 级', sse: '8 px', texture: 'BC7', root: '/Data/Tile_0' },
          wireframe: false
        })
      ],
      selected: 'osgb'
    },
    4: {
      mode: '轨道相机', camera: { scale: 1.05, rot: 20 },
      coord: { lon: 121.4751, lat: 31.2299, h: 0.12 },
      settings: { render: 'wireframe', grid: false, exag: 1.0, bg: 'space' },
      layers: [
        mkLayer('gltf', 'gltf', '设备模型', 'glTF 2.0 · 就绪', {
          meta: { format: 'glTF 2.0', meshes: '18', materials: '6', animations: '2', bounds: '42 × 18 × 12 m' },
          wireframe: false
        })
      ],
      selected: 'gltf'
    }
  };

  const state = {
    active: 1,
    maximized: false,
    detached: new Set(),
    selected: {},
    drag: null,
    addSource: 'local',
    addCounter: 1,
    menuId: null,
    panePopKind: null,
    panePopId: null,
    scriptRunning: false,
    scriptHeight: 300,
    stackRatio: 0.55,
    panels: {
      props: { open: true, mode: 'docked' },
      ai: { open: false, mode: 'docked' },
      script: { open: false, mode: 'docked' }
    },
    floatZ: 90,
    aiModels: [
      { id: 'local', name: '本地规则演示', provider: '内置', model: 'local-demo', base: '', key: '', context: '8K' }
    ],
    activeModel: 'local',
    modelSeq: 1,
    aiModeIndex: 0,
    platform: 'desktop',
    scriptAiDraft: false
  };

  let nextId = 1;
  let splitSeq = 0;
  const viewports = new Map();
  let tree = null;

  const app = $('#app');
  const grid = $('#vpGrid');
  const settingsContent = $('#settingsContent');
  const vpPicker = $('#vpPicker');
  const pickerList = $('#vpPickerList');
  const detachedLayer = $('#detachedLayer');
  const paneMenu = $('#paneMenu');
  const panePop = $('#panePop');
  const sideDock = $('#sideDock');
  const propSlot = $('#propSlot');
  const aiSlot = $('#aiSlot');
  const scriptSlot = $('#scriptSlot');
  const propPanel = $('#propPanel');
  const aiPanel = $('#aiPanel');
  const scriptPanel = $('#scriptPanel');
  const aiModelButton = $('#aiModelButton');
  const aiModelName = $('#aiModelName');
  const aiModelBadge = $('#aiModelBadge');
  const aiModelPop = $('#aiModelPop');
  const scriptAiBanner = $('#scriptAiBanner');
  const scriptCode = $('#scriptCode');
  const scriptLines = $('#scriptLines');
  const scriptDirty = $('#scriptDirty');
  const floatLayer = $('#floatLayer');
  const stackResizer = $('#stackResizer');
  const sideResizer = $('.side-resizer');
  const mainStack = $('#mainStack');
  const viewpoints = [
    { name: '城市中心', lon: 121.4737, lat: 31.2304, h: 8.42 },
    { name: '园区俯视', lon: 121.4702, lat: 31.2286, h: 0.42 }
  ];

  const activeVp = () => viewports.get(state.active) || Array.from(viewports.values())[0];
  const typeInfo = (type) => TYPE[type] || TYPE.tiles;
  const leafNode = (id) => ({ type: 'leaf', id });
  const splitNode = (orientation, a, b, ratio = 0.5) => ({ type: 'split', id: `s${++splitSeq}`, orientation, ratio, children: [a, b] });

  function createViewport(seedKey = null) {
    const id = nextId++;
    const seed = seedKey ? SEEDS[seedKey] : null;
    const vp = {
      id,
      name: `视口 ${id}`,
      mode: seed ? seed.mode : '轨道相机',
      camera: seed ? { ...seed.camera } : { scale: 1, rot: 0 },
      coord: seed ? { ...seed.coord } : { lon: 121.4737, lat: 31.2304, h: 8.42 },
      settings: { ...DEFAULT_SETTINGS, ...(seed ? seed.settings : {}) },
      layers: seed ? [globeLayer(), ...seed.layers.map((l) => ({ ...l }))] : [],
      treeOpen: false,
      collapsed: new Set()
    };
    viewports.set(id, vp);
    state.selected[id] = seed && seed.selected ? seed.selected : null;
    return vp;
  }

  function walkLeaves(node, out = []) {
    if (!node) return out;
    if (node.type === 'leaf') out.push(node);
    else node.children.forEach((child) => walkLeaves(child, out));
    return out;
  }
  function findSplit(node, id) {
    if (!node || node.type === 'leaf') return null;
    if (node.id === id) return node;
    return findSplit(node.children[0], id) || findSplit(node.children[1], id);
  }
  function replaceLeaf(node, targetId, replacement) {
    if (node.type === 'leaf') return node.id === targetId ? replacement : node;
    node.children = node.children.map((child) => replaceLeaf(child, targetId, replacement));
    return node;
  }
  function removeLeaf(node, targetId) {
    if (node.type === 'leaf') return node.id === targetId ? null : node;
    const kids = node.children.map((child) => removeLeaf(child, targetId)).filter(Boolean);
    if (kids.length === 0) return null;
    if (kids.length === 1) return kids[0];
    node.children = kids;
    return node;
  }

  let toastTimer = null;
  function toast(text) {
    const el = $('#toast');
    el.textContent = text;
    el.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => el.classList.remove('show'), 2400);
  }

  function layerIcon(layer) {
    const t = typeInfo(layer.type);
    return `<span class="type-ic ${t.cls}"><svg class="ic"><use href="#i-${t.icon}"/></svg></span>`;
  }
  function stateCell(layer) {
    if (layer.state === 'loading') return `<span class="state-ring" style="--p:${layer.progress || 0}"></span>`;
    if (layer.state === 'paused') return `<span class="state-paused"><svg class="ic"><use href="#i-pause"/></svg></span>`;
    return '<span class="state-dot"></span>';
  }
  const sharedCount = (key) => Array.from(viewports.values()).filter((vp) => vp.layers.some((l) => l.key === key)).length;
  const layerOf = (vp, key) => (vp ? vp.layers.find((l) => l.key === key) : null);

  function layerRowHtml(vp, layer) {
    const selected = state.selected[vp.id] === layer.key;
    const off = layer.visible === false ? 'is-off' : '';
    const shared = layer.key !== 'globe' && sharedCount(layer.key) > 1
      ? `<span class="shared-badge"><svg class="ic"><use href="#i-link"/></svg>共享 · ${sharedCount(layer.key)}</span>` : '';
    return `<div class="layer-row ${selected ? 'is-active-layer' : ''} ${off}" draggable="true" data-layer="${layer.key}" data-vp="${vp.id}" title="拖到其他视口可共享数据">
      <button class="eye-btn" type="button" data-eye="${layer.key}" aria-label="切换显隐" aria-pressed="${layer.visible !== false}"><svg class="ic"><use href="#i-eye"/></svg></button>
      ${layerIcon(layer)}
      <span class="layer-main">
        <span class="layer-name"><span class="name-text">${esc(layer.name)}</span>${shared}</span>
        <span class="layer-sub">${esc(layer.sub)}</span>
      </span>
      <span class="layer-state">${stateCell(layer)}</span>
    </div>`;
  }

  function treeNodeHtml(vp, key, label, count, icon, collapsed, extraClass = '') {
    return `<div class="tree-node ${extraClass} ${collapsed ? 'is-collapsed' : ''}" data-tree-node="${key}" data-vp="${vp.id}">
      <span class="tree-chevron"><svg class="ic"><use href="#i-chevron-down"/></svg></span>
      <svg class="ic node-ic"><use href="#i-${icon}"/></svg>
      <span class="node-label">${label}</span>
      <span class="tree-node-count">${count}</span>
    </div>`;
  }

  function paneTree(vp) {
    const collapsed = (key) => vp.collapsed.has(key);
    let body = `<div class="tree-node tree-camera ${state.selected[vp.id] === 'camera' ? 'is-selected' : ''}" data-camera="${vp.id}" title="相机属性">
      <span class="tree-camera-ic"><svg class="ic"><use href="#i-orbit"/></svg></span>
      <span class="node-label">相机</span>
      <span class="tree-camera-mode">${vp.mode}</span>
    </div>`;
    TREE_GROUPS.forEach((group) => {
      const items = vp.layers.filter((layer) => group.types.includes(layer.type));
      if (!items.length) return;
      body += treeNodeHtml(vp, group.id, group.label, items.length, group.icon, collapsed(group.id), 'tree-group');
      if (!collapsed(group.id)) {
        body += `<div class="tree-children">${items.map((layer) => layerRowHtml(vp, layer)).join('')}</div>`;
      }
    });
    return `<div class="pane-tree" data-tree-vp="${vp.id}">
      <div class="pane-tree-scroll">${body}</div>
      <div class="pane-tree-foot"><svg class="ic"><use href="#i-link"/></svg><span>拖到其他视口共享 · 会话不保存</span></div>
    </div>`;
  }

  function paneScene(vp) {
    const s = vp.settings;
    const hasData = vp.layers.length > 0;
    const hasGlobe = vp.layers.some((l) => l.type === 'globe');
    const lon = Math.abs(vp.coord.lon).toFixed(4);
    const lat = Math.abs(vp.coord.lat).toFixed(4);
    if (!hasData) {
      return `<div class="vp-scene scene-bg-${s.bg}" data-render="${s.render}" data-light="${s.light}">
        <div class="pane-empty-full">
          <span class="empty-orb"><svg class="ic"><use href="#i-globe"/></svg></span>
          <b>空视口</b>
          <p>添加 WGS84 椭球作为基础地球，或添加影像、地形和三维数据。</p>
          <button class="btn sm primary" type="button" data-pane-add="${vp.id}"><svg class="ic"><use href="#i-plus"/></svg>添加数据</button>
        </div>
      </div>`;
    }
    return `<div class="vp-scene scene-bg-${s.bg}" data-render="${s.render}" data-light="${s.light}">
      ${hasGlobe ? `<div class="globe-stage" style="--cam-scale:${vp.camera.scale};--cam-rot:${vp.camera.rot}deg">
        <svg class="globe" viewBox="0 0 640 640" aria-hidden="true"><use href="#art-globe"/></svg>
      </div>` : ''}
      <div class="pane-hud">${lon}°E ${lat}°N · <b>${vp.coord.h.toFixed(2)} km</b> · <span class="exag">${s.exag.toFixed(1)}×</span></div>
      <div class="pane-attrib">示例数据 · L17</div>
    </div>`;
  }

  function detachedPlaceholder(vp) {
    return `<div class="pane-detached">
      <span class="empty-ic"><svg class="ic"><use href="#i-detach"/></svg></span>
      <b>${vp.name} 已在独立窗口</b>
      <p>该视口的数据、相机和设置继续独立运行；可停靠回主窗口。</p>
      <button class="btn sm" type="button" data-pane-act="dock" data-vp="${vp.id}">停靠回主窗口</button>
    </div>`;
  }

  function paneActions(vp) {
    const detached = state.detached.has(vp.id);
    if (detached) {
      return `<span class="vp-actions">
        <button class="icon-btn wide-only" type="button" data-pane-act="dock" data-vp="${vp.id}" title="停靠回主窗口"><svg class="ic"><use href="#i-refresh"/></svg></button>
        <button class="icon-btn wide-only danger" type="button" data-pane-act="close" data-vp="${vp.id}" title="关闭视口"><svg class="ic"><use href="#i-x"/></svg></button>
        <button class="icon-btn vp-more" type="button" data-pane-menu="${vp.id}" title="更多"><svg class="ic"><use href="#i-more-h"/></svg></button>
      </span>`;
    }
    return `<span class="vp-actions">
      <button class="icon-btn wide-only" type="button" data-pane-act="split-h" data-vp="${vp.id}" title="左右分割"><svg class="ic"><use href="#i-split-h"/></svg></button>
      <button class="icon-btn wide-only" type="button" data-pane-act="split-v" data-vp="${vp.id}" title="上下分割"><svg class="ic"><use href="#i-split-v"/></svg></button>
      <button class="icon-btn wide-only" type="button" data-pane-act="detach" data-vp="${vp.id}" title="分离为独立窗口"><svg class="ic"><use href="#i-detach"/></svg></button>
      <button class="icon-btn wide-only" type="button" data-pane-act="max" data-vp="${vp.id}" title="${state.maximized ? '退出全屏视口' : '全屏视口'}"><svg class="ic"><use href="#i-${state.maximized ? 'minimize' : 'maximize'}"/></svg></button>
      <button class="icon-btn wide-only danger" type="button" data-pane-act="close" data-vp="${vp.id}" title="关闭视口"><svg class="ic"><use href="#i-x"/></svg></button>
      <button class="icon-btn vp-more" type="button" data-pane-menu="${vp.id}" title="更多"><svg class="ic"><use href="#i-more-h"/></svg></button>
    </span>`;
  }

  function paneHtml(vp) {
    const detached = state.detached.has(vp.id);
    const hasGlobe = vp.layers.some((l) => l.type === 'globe');
    const hasShared = vp.layers.some((l) => l.key !== 'globe' && sharedCount(l.key) > 1);
    return `<article class="vp-pane ${vp.id === state.active ? 'is-active' : ''} ${detached ? 'is-detached' : ''}" data-vp="${vp.id}" tabindex="0" aria-label="${vp.name}">
      <header class="vp-header" data-vp="${vp.id}">
        <div class="vp-left">
          <button class="tree-btn ${vp.treeOpen ? 'is-open' : ''}" type="button" data-tree-toggle="${vp.id}" title="图层树" aria-label="打开图层树" aria-expanded="${vp.treeOpen}">
            <svg class="ic"><use href="#i-layers"/></svg><span class="tree-num">${vp.layers.length}</span>${hasShared ? '<span class="tree-shared-dot"></span>' : ''}
          </button>
          <button class="add-pane-btn" type="button" data-pane-add="${vp.id}" title="添加数据" aria-label="添加数据"><svg class="ic"><use href="#i-plus"/></svg></button>
          ${detached ? '' : `<span class="vp-tool-sep"></span>
          <button class="vp-tool" type="button" data-pane-tool="fly" data-vp="${vp.id}" title="飞往坐标" aria-label="飞往坐标"><svg class="ic"><use href="#i-navigation"/></svg></button>
          <button class="vp-tool" type="button" data-pane-tool="viewpoints" data-vp="${vp.id}" title="会话视点" aria-label="会话视点"><svg class="ic"><use href="#i-bookmark"/></svg><span class="vp-tool-badge">${viewpoints.length}</span></button>
          <button class="vp-tool" type="button" data-pane-tool="display" data-vp="${vp.id}" title="显示设置" aria-label="显示设置"><svg class="ic"><use href="#i-sliders"/></svg></button>
          <button class="vp-tool ${vp.settings.grid ? 'is-on' : ''}" type="button" data-pane-tool="grid" data-vp="${vp.id}" title="${hasGlobe ? '经纬网' : '添加 WGS84 椭球后可用'}" aria-label="经纬网" ${hasGlobe ? '' : 'disabled'}><svg class="ic"><use href="#i-grid"/></svg></button>`}
        </div>
        <span class="vp-title">${vp.name}</span>
        ${paneActions(vp)}
      </header>
      ${detached ? detachedPlaceholder(vp) : paneScene(vp)}
      ${vp.treeOpen ? paneTree(vp) : ''}
      <div class="pane-drop">共享到此视口</div>
    </article>`;
  }

  function renderNode(node) {
    if (!node) return '';
    if (node.type === 'leaf') {
      const vp = viewports.get(node.id);
      return vp ? paneHtml(vp) : '';
    }
    return `<div class="split-node" data-orient="${node.orientation}" data-node="${node.id}" style="--ratio:${node.ratio}">
      <div class="split-cell">${renderNode(node.children[0])}</div>
      <div class="split-divider" data-divider="${node.id}" role="separator" aria-orientation="${node.orientation === 'h' ? 'vertical' : 'horizontal'}"></div>
      <div class="split-cell">${renderNode(node.children[1])}</div>
    </div>`;
  }

  function renderPanes() {
    if (state.maximized) {
      const vp = activeVp();
      grid.innerHTML = `<div class="split-cell">${paneHtml(vp)}</div>`;
    } else {
      grid.innerHTML = renderNode(tree);
    }
    app.classList.toggle('vp-max', state.maximized);
  }

  function sectionLabel(text) { return `<div class="section-label">${text}</div>`; }
  function stateText(layer) {
    if (layer.state === 'loading') return `加载 ${layer.progress || 0}%`;
    if (layer.state === 'paused') return '已暂停';
    return '就绪';
  }
  function kv(k, v) { return `<div class="prop-kv"><span>${k}</span><b>${v}</b></div>`; }
  function switchRow(label, key, on) {
    return `<div class="row-between"><span>${label}</span><button class="switch ${on ? 'is-on' : ''}" type="button" data-layer-toggle="${key}" role="switch" aria-checked="${!!on}"></button></div>`;
  }
  function rangeRow(label, key, value, min, max, step, display) {
    return `<div class="range-label"><span>${label}</span><span class="value-pill">${display}</span></div><input type="range" min="${min}" max="${max}" step="${step}" value="${value}" data-layer-set="${key}">`;
  }

  function setPropHeader(title, sub, chipText, showReset) {
    $('#vpTitle').textContent = title;
    $('#propSub').textContent = sub;
    const chip = $('#propChip');
    chip.className = showReset ? 'chip ok' : 'chip type-chip';
    chip.textContent = chipText;
    const action = $('#propAction');
    action.hidden = !showReset;
    action.dataset.act = 'reset-vp';
    action.title = '恢复该视口默认设置';
    action.setAttribute('aria-label', action.title);
    action.querySelector('use').setAttribute('href', '#i-refresh');
  }

  function renderPropHeader(vp, layer) {
    if (layer) {
      setPropHeader(layer.name, `${TYPE[layer.type].label} · ${stateText(layer)} · ${vp.name}`, TYPE[layer.type].label, false);
    } else {
      setPropHeader(vp.name, '视口属性 · 布局与环境', '独立', true);
    }
  }

  function renderCameraHeader(vp) {
    setPropHeader('相机', `CAMERA · ${vp.mode} · ${vp.name}`, 'CAMERA', false);
  }

  function layerData(layer) {
    const m = layer.meta || {};
    const t = layer.type;
    if (t === 'tiles') {
      return kv('格式', m.format || '3D Tiles 1.1') + kv('瓦片', m.tiles || '—') + kv('几何误差', m.geometricError || '—') + kv('屏幕空间误差', m.sse || '—') + kv('内存', m.memory || '—');
    }
    if (t === 'osgb') {
      return kv('格式', m.format || 'OSGB / PagedLOD') + kv('LOD 层数', m.lod || '—') + kv('最大屏幕误差', m.sse || '—') + kv('纹理压缩', m.texture || '—') + kv('根节点', m.root || '—');
    }
    if (t === 'image') {
      return kv('格式', m.format || 'XYZ') + kv('坐标参考', m.crs || '—') + kv('层级范围', m.zoom || '—') + kv('瓦片', m.tiles || '—') + kv('版权', m.attribution || '—');
    }
    if (t === 'terrain') {
      return kv('格式', m.format || 'COG') + kv('分辨率', m.resolution || '—') + kv('高程范围', m.elevation || '—') + kv('NoData', m.nodata || '—') + kv('坐标参考', m.crs || '—');
    }
    if (t === 'points') {
      return kv('格式', m.format || 'PNTS') + kv('点数', m.points || '—') + kv('点间距', m.spacing || '—') + kv('颜色', m.color || '—') + kv('预算', m.budget || '—');
    }
    if (t === 'gltf') {
      return kv('格式', m.format || 'glTF 2.0') + kv('网格', m.meshes || '—') + kv('材质', m.materials || '—') + kv('动画', m.animations || '—') + kv('包围盒', m.bounds || '—');
    }
    if (t === 'script') {
      return kv('语言', 'GDScript') + kv('文件', m.file || '—') + kv('入口', m.entry || 'run(viewer)') + kv('权限', m.permissions || '应用级') + kv('状态', stateText(layer));
    }
    return kv('椭球', m.format || 'WGS84') + kv('长半轴', m.semiMajor || '—') + kv('扁率', m.flattening || '—') + kv('基准', m.datum || '—');
  }

  function layerDisplay(layer) {
    if (layer.type === 'globe') return '';
    let html = switchRow('可见', 'visible', layer.visible !== false);
    html += rangeRow('透明度', 'opacity', Math.round((layer.opacity ?? 1) * 100), 0, 100, 1, `${Math.round((layer.opacity ?? 1) * 100)}%`);
    if (layer.type === 'image') {
      html += rangeRow('亮度', 'brightness', layer.brightness ?? 1, 0.2, 2, 0.05, `${(layer.brightness ?? 1).toFixed(2)}×`);
      html += rangeRow('对比度', 'contrast', layer.contrast ?? 1, 0.2, 2, 0.05, `${(layer.contrast ?? 1).toFixed(2)}×`);
    } else if (layer.type === 'points') {
      html += rangeRow('点大小', 'pointSize', layer.pointSize ?? 2.4, 1, 8, 0.1, `${(layer.pointSize ?? 2.4).toFixed(1)} px`);
      html += `<div class="field-label">颜色模式</div><div class="seg wide">${['RGB', 'Intensity', 'Classification'].map((mode) => `<button type="button" data-layer-color="${mode}" class="${(layer.colorMode || 'RGB') === mode ? 'is-on' : ''}">${mode}</button>`).join('')}</div>`;
    } else if (layer.type === 'tiles') {
      html += switchRow('显示轮廓', 'outline', !!layer.outline);
    } else if (layer.type === 'osgb' || layer.type === 'gltf') {
      html += switchRow('线框', 'wireframe', !!layer.wireframe);
    } else if (layer.type === 'terrain') {
      html += switchRow('参与地形', 'terrainActive', layer.terrainActive !== false);
      html += rangeRow('地形夸张', 'exag', layer.exag ?? 1.5, 0, 10, 0.1, `${(layer.exag ?? 1.5).toFixed(1)}×`);
    }
    return html;
  }

  function renderLayerSettings(vp, layer) {
    const isGlobe = layer.type === 'globe';
    if (layer.type === 'script') {
      return `
        <div class="prop-group">${sectionLabel('脚本')}${layerData(layer)}</div>
        <div class="prop-group">${sectionLabel('运行')}
          <div class="setting-note"><svg class="ic"><use href="#i-shield"/></svg><span>脚本拥有应用权限；运行前请审查，同步死循环无法停止。</span></div>
        </div>
        <div class="prop-actions">
          <button class="btn sm" type="button" data-layer-action="open-script">打开脚本</button>
          <button class="btn sm primary" type="button" data-layer-action="run-script">运行</button>
          <button class="btn sm danger" type="button" data-layer-action="remove">移除</button>
        </div>
        <div class="prop-group">${sectionLabel('来源')}${kv('添加方式', '添加数据 → GDScript 脚本')}${kv('范围', '应用级')}</div>`;
    }
    const refs = isGlobe ? 1 : sharedCount(layer.key);
    const sharedNote = !isGlobe && refs > 1
      ? `<div class="setting-note"><svg class="ic"><use href="#i-link"/></svg><span>数据源属性影响 ${refs} 个视口；本视口显示属性只影响 ${vp.name}。</span></div>`
      : '';
    const actions = isGlobe ? '' : `<div class="prop-actions">
      <button class="btn sm" type="button" data-layer-action="reload">重新加载</button>
      <button class="btn sm" type="button" data-layer-action="pause">${layer.state === 'paused' ? '恢复' : '暂停'}</button>
      <button class="btn sm danger" type="button" data-layer-action="remove">移除</button>
    </div>`;
    const globeDisplay = isGlobe ? `<div class="prop-group">${sectionLabel('显示')}
      <div class="row-between"><span>经纬网</span><button class="switch ${vp.settings.grid ? 'is-on' : ''}" type="button" data-globe-toggle="grid" role="switch" aria-checked="${vp.settings.grid}"></button></div>
    </div>` : '';
    return `
      <div class="prop-group">${sectionLabel(isGlobe ? '椭球' : refs > 1 ? '共享数据' : '数据源')}${layerData(layer)}</div>
      ${isGlobe ? globeDisplay : `<div class="prop-group">${sectionLabel('本视口显示')}${layerDisplay(layer)}</div>`}
      <div class="prop-group">${sectionLabel('状态')}${kv('状态', stateText(layer))}${layer.state === 'loading' ? kv('进度', `${layer.progress || 0}%`) : ''}${isGlobe ? '' : kv('共享引用', refs)}</div>
      ${actions}
      ${sharedNote}
    `;
  }

  function renderViewportSettings(vp) {
    const pane = document.querySelector(`.vp-pane[data-vp="${vp.id}"]`);
    const rect = pane ? pane.getBoundingClientRect() : null;
    const leaves = walkLeaves(tree);
    const index = leaves.findIndex((leaf) => leaf.id === vp.id);
    const size = rect ? `${Math.round(rect.width)} × ${Math.round(rect.height)} px` : '—';
    return `
      <div class="setting-note"><svg class="ic"><use href="#i-info"/></svg><span>视口只保留布局与环境属性；相机、地形、经纬网分别属于相机对象、DEM 图层和地球对象。</span></div>
      <div class="prop-group">${sectionLabel('视口')}
        ${kv('名称', vp.name)}
        ${kv('布局', `${leaves.length} 视口 · 第 ${index + 1} 个`)}
        ${kv('尺寸', size)}
        ${kv('窗口', state.detached.has(vp.id) ? '独立窗口' : '主窗口')}
        ${kv('状态', vp.id === state.active ? '活动' : '非活动')}
      </div>
      <div class="prop-group">${sectionLabel('环境')}
        <div class="seg wide" data-render-seg>
          <button type="button" data-render-opt="textured" class="${vp.settings.render === 'textured' ? 'is-on' : ''}">材质</button>
          <button type="button" data-render-opt="unlit" class="${vp.settings.render === 'unlit' ? 'is-on' : ''}">无光照</button>
          <button type="button" data-render-opt="wireframe" class="${vp.settings.render === 'wireframe' ? 'is-on' : ''}">线框</button>
        </div>
        <label class="field" style="margin-top:8px"><span>背景</span><select data-set="bg">
          <option value="space" ${vp.settings.bg === 'space' ? 'selected' : ''}>深空</option>
          <option value="warm" ${vp.settings.bg === 'warm' ? 'selected' : ''}>晨昏</option>
          <option value="flat" ${vp.settings.bg === 'flat' ? 'selected' : ''}>纯色</option>
        </select></label>
        <label class="field" style="margin-top:8px"><span>光照预设</span><select data-set="light">
          <option value="default" ${vp.settings.light === 'default' ? 'selected' : ''}>默认</option>
          <option value="bright" ${vp.settings.light === 'bright' ? 'selected' : ''}>明亮</option>
          <option value="night" ${vp.settings.light === 'night' ? 'selected' : ''}>夜间</option>
        </select></label>
      </div>
      <div class="prop-group">${sectionLabel('性能')}
        ${kv('活动帧率', '60 FPS')}
        ${kv('非活动帧率', '30 FPS')}
        ${kv('渲染比例', '100%')}
        ${kv('MSAA', '4×')}
      </div>
    `;
  }

  function renderCameraSettings(vp) {
    const cam = vp.camera;
    return `
      <div class="prop-group">${sectionLabel('相机')}
        <div class="field-label">模式</div>
        <div class="seg wide">
          <button type="button" data-camera-mode="轨道相机" class="${vp.mode === '轨道相机' ? 'is-on' : ''}">轨道</button>
          <button type="button" data-camera-mode="飞行相机" class="${vp.mode === '飞行相机' ? 'is-on' : ''}">飞行</button>
        </div>
        <label class="field" style="margin-top:8px"><span>投影</span><select data-camera-set="projection">
          <option ${vp.settings.projection === '透视' ? 'selected' : ''}>透视</option>
          <option ${vp.settings.projection === '正交' ? 'selected' : ''}>正交</option>
        </select></label>
        <div class="range-label"><span>视场角 FOV</span><span class="value-pill">${vp.settings.fov}°</span></div>
        <input type="range" min="30" max="90" step="1" value="${vp.settings.fov}" data-camera-set="fov">
        ${kv('近裁剪', '0.1 m')}
        ${kv('远裁剪', '20,000 km')}
      </div>
      <div class="prop-group">${sectionLabel('当前视点')}
        ${kv('经度', `${Math.abs(vp.coord.lon).toFixed(4)}°E`)}
        ${kv('纬度', `${Math.abs(vp.coord.lat).toFixed(4)}°N`)}
        ${kv('高度', `${vp.coord.h.toFixed(2)} km`)}
        ${kv('旋转', `${cam.rot}°`)}
        ${kv('视距', `${cam.scale.toFixed(2)}×`)}
      </div>
      <div class="prop-actions"><button class="btn sm" type="button" data-camera-action="reset">重置相机</button></div>
    `;
  }

  function renderSettings() {
    const vp = activeVp();
    const selected = state.selected[vp.id];
    if (selected === 'camera') {
      renderCameraHeader(vp);
      $('#settingsContent').innerHTML = renderCameraSettings(vp);
      return;
    }
    const layer = layerOf(vp, selected);
    renderPropHeader(vp, layer || null);
    $('#settingsContent').innerHTML = layer ? renderLayerSettings(vp, layer) : renderViewportSettings(vp);
  }

  function renderPicker() {
    const leaves = walkLeaves(tree);
    pickerList.innerHTML = leaves.map((leaf, index) => {
      const vp = viewports.get(leaf.id);
      if (!vp) return '';
      const detached = state.detached.has(vp.id);
      return `<button class="vp-picker-item ${vp.id === state.active ? 'is-on' : ''}" type="button" data-pick="${vp.id}">
        <span class="dot"></span>
        <span><b>${vp.name}${vp.id === state.active ? ' · 活动' : ''}</b><small>${index + 1} · ${vp.layers.length} 图层 · ${detached ? '独立窗口' : '主窗口'}</small></span>
        ${detached ? '<span class="win">独立</span>' : ''}
      </button>`;
    }).join('');
  }

  function renderPanePop() {
    if (!state.panePopKind || !viewports.has(state.panePopId)) {
      panePop.hidden = true;
      return;
    }
    const vp = viewports.get(state.panePopId);
    panePop.hidden = false;
    if (state.panePopKind === 'display') {
      panePop.innerHTML = `<div class="pane-pop-head"><strong>显示</strong><span>${vp.name}</span></div>
        <div class="seg wide">${[['textured', '材质'], ['unlit', '无光照'], ['wireframe', '线框']].map(([v, label]) => `<button type="button" data-pane-render="${v}" class="${vp.settings.render === v ? 'is-on' : ''}">${label}</button>`).join('')}</div>
        <div class="pane-pop-foot"><button class="btn wide" type="button" data-pane-close>完成</button></div>`;
    } else {
      panePop.innerHTML = `<div class="pane-pop-head"><strong>会话视点</strong><span>${vp.name}</span></div>
        ${viewpoints.map((p, i) => `<button class="pop-item" type="button" data-pane-viewpoint="${i}"><span class="pop-ic"><svg class="ic"><use href="#i-bookmark"/></svg></span><span><strong>${p.name}</strong><small>${Math.abs(p.lon).toFixed(4)}°E ${Math.abs(p.lat).toFixed(4)}°N · ${p.h.toFixed(2)} km</small></span></button>`).join('')}
        <div class="pane-pop-foot"><button class="btn wide" type="button" data-pane-save-vp><svg class="ic"><use href="#i-bookmark-plus"/></svg>保存当前视点</button></div>`;
    }
    positionPanePop();
  }

  function positionPanePop() {
    if (panePop.hidden || !state.panePopId) return;
    const pane = $(`.vp-pane[data-vp="${state.panePopId}"]`);
    if (!pane) return;
    const anchor = (() => {
      const tool = pane.querySelector(`[data-pane-tool="${state.panePopKind}"]`);
      if (tool && tool.getClientRects().length) return tool;
      return pane.querySelector('[data-pane-menu]') || pane;
    })();
    const ar = app.getBoundingClientRect();
    const br = anchor.getBoundingClientRect();
    let left = br.left + br.width / 2 > ar.width / 2
      ? br.right - ar.left - 244
      : br.left - ar.left - 4;
    let top = br.bottom - ar.top + 4;
    left = clamp(left, 8, Math.max(8, ar.width - 252));
    if (top + panePop.offsetHeight > ar.height - 8) top = br.top - ar.top - panePop.offsetHeight - 4;
    panePop.style.left = `${left}px`;
    panePop.style.top = `${Math.max(8, top)}px`;
  }

  function openPanePop(kind, id, anchor) {
    if (!viewports.has(id)) return;
    if (state.panePopKind === kind && state.panePopId === id) { closePanePop(); return; }
    state.panePopKind = kind;
    state.panePopId = id;
    state.active = id;
    renderAll();
  }

  function closePanePop() {
    state.panePopKind = null;
    state.panePopId = null;
    panePop.hidden = true;
  }

  function renderAIModels() {
    if (!aiModelButton) return;
    const active = state.aiModels.find((m) => m.id === state.activeModel) || state.aiModels[0];
    state.activeModel = active.id;
    aiModelName.textContent = active.name;
    aiModelBadge.textContent = active.context || '—';
    aiModelButton.title = `${active.name} · ${active.provider}${active.context ? ` · ${active.context}` : ''}`;
    aiModelPop.innerHTML = `<div class="ai-model-pop-head"><span>选择模型</span><button type="button" data-ai-add-model>+ 添加模型</button></div>
      ${state.aiModels.map((m) => `<button class="ai-model-item ${m.id === active.id ? 'is-on' : ''}" type="button" data-model-pick="${esc(m.id)}">
        <svg class="ic ai-model-check"><use href="#i-check"/></svg>
        <div><b>${esc(m.name)}</b><small>${esc(m.provider)}${m.context ? ` · ${esc(m.context)}` : ''}</small></div>
      </button>`).join('')}`;
  }

  function openModelPop() {
    if (!aiModelButton) return;
    const r = aiModelButton.getBoundingClientRect();
    aiModelPop.hidden = false;
    const width = 252;
    aiModelPop.style.left = `${clamp(r.left, 8, window.innerWidth - width - 8)}px`;
    aiModelPop.style.bottom = `${Math.max(8, window.innerHeight - r.top + 7)}px`;
    aiModelPop.style.top = 'auto';
  }

  function closeModelPop() { aiModelPop.hidden = true; }

  function renderTargets() {
    $('#runScript').disabled = state.scriptRunning;
    $('#stopScript').disabled = !state.scriptRunning;
  }

  function ensureFloat(panel, name) {
    if (!panel.querySelector('.float-resize')) {
      const grip = document.createElement('div');
      grip.className = 'float-resize';
      grip.dataset.floatResize = name;
      panel.appendChild(grip);
    }
    if (!panel.style.left) {
      const defaults = {
        props: { left: 420, top: 70, w: 380, h: 560 },
        ai: { left: 830, top: 70, w: 340, h: 420 },
        script: { left: 400, top: 300, w: 760, h: 380 }
      };
      const d = defaults[name];
      panel.style.left = `${d.left}px`;
      panel.style.top = `${d.top}px`;
      panel.style.width = `${d.w}px`;
      panel.style.height = `${d.h}px`;
      panel.style.zIndex = String(++state.floatZ);
    }
  }

  function renderPanels() {
    const P = state.panels;
    const leftOpen = (P.props.open && P.props.mode === 'docked') || (P.ai.open && P.ai.mode === 'docked');
    app.classList.toggle('no-side', !leftOpen);
    sideDock.hidden = !leftOpen;
    sideResizer.hidden = !leftOpen;

    const defs = [
      { name: 'props', panel: propPanel, slot: propSlot },
      { name: 'ai', panel: aiPanel, slot: aiSlot },
      { name: 'script', panel: scriptPanel, slot: scriptSlot }
    ];
    defs.forEach(({ name, panel, slot }) => {
      const st = P[name];
      panel.hidden = !st.open;
      if (!st.open) {
        slot.hidden = true;
        return;
      }
      const floating = st.mode === 'floating';
      panel.classList.toggle('is-floating', floating);
      if (floating) {
        slot.hidden = true;
        if (panel.parentElement !== floatLayer) floatLayer.appendChild(panel);
        ensureFloat(panel, name);
      } else {
        if (panel.parentElement !== slot) slot.appendChild(panel);
        const grip = panel.querySelector('.float-resize');
        if (grip) grip.remove();
        panel.style.left = '';
        panel.style.top = '';
        panel.style.width = '';
        panel.style.height = '';
        panel.style.zIndex = '';
        slot.hidden = false;
      }
      const action = panel.querySelector('[data-panel-action^="detach-"], [data-panel-action^="dock-"]');
      if (action) {
        action.dataset.panelAction = floating ? `dock-${name}` : `detach-${name}`;
        action.title = floating ? '停靠回工作区' : '独立窗口';
        action.setAttribute('aria-label', action.title);
        action.querySelector('use').setAttribute('href', floating ? '#i-dock' : '#i-detach');
      }
    });

    propSlot.hidden = !P.props.open || P.props.mode !== 'docked';
    aiSlot.hidden = !P.ai.open || P.ai.mode !== 'docked';
    stackResizer.hidden = !(P.props.open && P.props.mode === 'docked' && P.ai.open && P.ai.mode === 'docked');
    const stackBoth = !stackResizer.hidden;
    propSlot.style.flex = stackBoth ? String(state.stackRatio) : '1';
    aiSlot.style.flex = stackBoth ? String(1 - state.stackRatio) : '1';
    scriptSlot.hidden = !(P.script.open && P.script.mode === 'docked');
    if (!scriptSlot.hidden) scriptSlot.style.height = `${state.scriptHeight}px`;

    $$('.topbar [data-act="props"], .topbar [data-act="ai"], .topbar [data-act="script"]').forEach((btn) => {
      const st = P[btn.dataset.act];
      btn.classList.toggle('is-on', st.open);
      btn.classList.toggle('is-detached', st.open && st.mode === 'floating');
    });
    $('#runScript').disabled = state.scriptRunning;
    $('#stopScript').disabled = !state.scriptRunning;
  }

  function renderTopbar() {
    // Camera mode is per-viewport and lives in the camera object panel.
  }

  function renderStatus() {
    const vp = activeVp();
    const leaves = walkLeaves(tree);
    const sharedSets = new Set();
    viewports.forEach((v) => v.layers.forEach((l) => { if (l.key !== 'globe' && sharedCount(l.key) > 1) sharedSets.add(l.key); }));
    $('#statusActive').textContent = `${vp.name} · 活动${state.detached.has(vp.id) ? ' · 独立窗口' : ''}`;
    $('#statusLayers').textContent = vp.layers.length;
    $('#statusShared').textContent = sharedSets.size;
    $('#statusVps').textContent = leaves.length;
  }

  function renderAll() {
    paneMenu.hidden = true;
    renderPanes();
    renderSettings();
    renderPicker();
    renderStatus();
    renderTopbar();
    renderTargets();
    renderAIModels();
    renderPanels();
    renderDetached();
    if (state.panePopKind) renderPanePop(); else panePop.hidden = true;
  }

  function setActive(id, clearSelection = false) {
    if (!viewports.has(id)) return;
    if (state.panePopId && state.panePopId !== id) closePanePop();
    if (clearSelection) state.selected[id] = null;
    state.active = id;
    renderAll();
  }

  function splitLeaf(id, orientation) {
    const vp = viewports.get(id);
    if (!vp) return;
    if (state.detached.has(id)) { toast('请先停靠该视口，再进行分割'); return; }
    state.maximized = false;
    closePanePop();
    const newVp = createViewport(null);
    tree = replaceLeaf(tree, id, splitNode(orientation, leafNode(id), leafNode(newVp.id), 0.5));
    state.active = newVp.id;
    renderAll();
    toast(`已分割 ${vp.name}；新视口为空，不影响其他视口`);
  }

  function closeLeaf(id) {
    const leaves = walkLeaves(tree);
    if (leaves.length <= 1) { toast('至少保留一个视口；请使用重置而不是关闭'); return; }
    const vp = viewports.get(id);
    if (!vp) return;
    if (state.panePopId === id) closePanePop();
    tree = removeLeaf(tree, id);
    viewports.delete(id);
    state.detached.delete(id);
    delete state.selected[id];
    if (!viewports.has(state.active)) {
      const remaining = walkLeaves(tree);
      state.active = remaining[0].id;
    }
    state.maximized = false;
    renderAll();
    toast(`${vp.name} 已关闭，仅合并该节点空间`);
  }

  function detach(id) {
    if (state.platform === 'web') { toast('Web 端不支持视口独立窗口'); return; }
    if (!viewports.has(id) || state.detached.has(id)) return;
    state.detached.add(id);
    state.maximized = false;
    renderAll();
    toast(`${viewports.get(id).name} 已分离到独立窗口`);
  }
  function dock(id) {
    if (!state.detached.has(id)) return;
    state.detached.delete(id);
    renderAll();
    toast(`${viewports.get(id).name} 已停靠回主窗口`);
  }
  function toggleMax() {
    if (state.detached.has(state.active)) { toast('独立窗口中的视口已独占窗口'); return; }
    closePanePop();
    state.maximized = !state.maximized;
    renderPanes();
    renderStatus();
    renderTopbar();
    toast(state.maximized ? '全屏活动视口 · Esc 退出' : '已恢复分屏');
  }

  function renderDetached() {
    const list = Array.from(state.detached).filter((id) => viewports.has(id));
    detachedLayer.innerHTML = list.map((id, index) => {
      const vp = viewports.get(id);
      return `<div class="detached-window" data-detached-id="${vp.id}" style="right:${20 + index * 26}px;bottom:${42 + index * 26}px">
        <div class="detached-head">
          <span class="win-ic"><svg class="ic"><use href="#i-detach"/></svg></span>
          <span class="detached-title">独立窗口 · ${vp.name}</span>
          <span class="detached-sub">由系统标题栏承载</span>
          <span class="grow"></span>
          <button class="btn sm" type="button" data-pane-act="dock" data-vp="${vp.id}">停靠</button>
        </div>
        <div class="detached-scene">
          <div class="globe-stage" style="--cam-scale:${vp.camera.scale};--cam-rot:${vp.camera.rot}deg">
            <svg class="globe" viewBox="0 0 640 640" aria-hidden="true"><use href="#art-globe"/></svg>
          </div>
          <div class="detached-hud">${Math.abs(vp.coord.lon).toFixed(4)}°E ${Math.abs(vp.coord.lat).toFixed(4)}°N · ${vp.coord.h.toFixed(2)} km</div>
        </div>
        <div class="detached-foot"><span>${vp.layers.length} 图层 · ${vp.settings.projection} · ${vp.settings.exag.toFixed(1)}×</span><span class="grow"></span><span>共享预算</span></div>
      </div>`;
    }).join('');
  }

  /* -------- pane menu -------- */
  function openPaneMenu(button, id) {
    const vp = viewports.get(id);
    if (!vp) return;
    const detached = state.detached.has(vp.id);
    paneMenu.innerHTML = detached
      ? `<button type="button" data-menu-act="dock"><svg class="ic"><use href="#i-refresh"/></svg>停靠回主窗口</button>
         <div class="menu-sep"></div>
         <button type="button" data-menu-act="close" class="danger"><svg class="ic"><use href="#i-x"/></svg>关闭视口</button>`
      : `<button type="button" data-menu-act="fly"><svg class="ic"><use href="#i-navigation"/></svg>飞往坐标</button>
         <button type="button" data-menu-act="viewpoints"><svg class="ic"><use href="#i-bookmark"/></svg>会话视点</button>
         <button type="button" data-menu-act="display"><svg class="ic"><use href="#i-sliders"/></svg>显示设置</button>
         <button type="button" data-menu-act="grid"><svg class="ic"><use href="#i-grid"/></svg>切换经纬网</button>
         <div class="menu-sep"></div>
         <button type="button" data-menu-act="split-h"><svg class="ic"><use href="#i-split-h"/></svg>左右分割</button>
         <button type="button" data-menu-act="split-v"><svg class="ic"><use href="#i-split-v"/></svg>上下分割</button>
         <button type="button" data-menu-act="detach"><svg class="ic"><use href="#i-detach"/></svg>分离为独立窗口</button>
         <button type="button" data-menu-act="max"><svg class="ic"><use href="#i-${state.maximized ? 'minimize' : 'maximize'}"/></svg>${state.maximized ? '退出全屏视口' : '全屏视口'}</button>
         <div class="menu-sep"></div>
         <button type="button" data-menu-act="close" class="danger"><svg class="ic"><use href="#i-x"/></svg>关闭视口</button>`;
    state.menuId = id;
    paneMenu.hidden = false;
    const br = button.getBoundingClientRect();
    const ar = app.getBoundingClientRect();
    let left = clamp(br.right - ar.left - 186, 8, ar.width - 198);
    let top = br.bottom - ar.top + 4;
    paneMenu.style.left = `${left}px`;
    paneMenu.style.top = `${top}px`;
    if (top + paneMenu.offsetHeight > ar.height - 8) {
      top = br.top - ar.top - paneMenu.offsetHeight - 4;
      paneMenu.style.top = `${Math.max(8, top)}px`;
    }
  }

  function hidePaneMenu() {
    paneMenu.hidden = true;
    state.menuId = null;
  }

  /* -------- settings -------- */
  settingsContent.addEventListener('change', (e) => {
    const camEl = e.target.closest('[data-camera-set]');
    if (camEl) {
      const vp = activeVp();
      if (camEl.tagName === 'SELECT') vp.settings.projection = camEl.value;
      else vp.settings.fov = Number(camEl.value);
      renderSettings();
      return;
    }
    const el = e.target.closest('[data-set]');
    if (!el) return;
    const vp = activeVp();
    const key = el.dataset.set;
    vp.settings[key] = key === 'fov' || key === 'exag' ? Number(el.value) : el.value;
    if (key === 'projection' || key === 'bg' || key === 'light') renderPanes();
    if (key === 'fov') { const label = $('#fovValue'); if (label) label.textContent = `${vp.settings.fov}°`; }
    if (key === 'exag') { const label = $('#exagValue'); if (label) label.textContent = `${vp.settings.exag.toFixed(1)}×`; }
    renderStatus();
  });
  settingsContent.addEventListener('input', (e) => {
    const camEl = e.target.closest('input[data-camera-set="fov"]');
    if (camEl) {
      const vp = activeVp();
      vp.settings.fov = Number(camEl.value);
      const label = camEl.previousElementSibling && camEl.previousElementSibling.querySelector('.value-pill');
      if (label) label.textContent = `${vp.settings.fov}°`;
      return;
    }
    const el = e.target.closest('input[data-set]');
    if (!el) return;
    const vp = activeVp();
    const key = el.dataset.set;
    if (key === 'fov') {
      vp.settings.fov = Number(el.value);
      const label = $('#fovValue'); if (label) label.textContent = `${vp.settings.fov}°`;
    } else if (key === 'exag') {
      vp.settings.exag = Number(el.value);
      const label = $('#exagValue'); if (label) label.textContent = `${vp.settings.exag.toFixed(1)}×`;
      const paneExag = $(`.vp-pane[data-vp="${vp.id}"] .pane-hud .exag`);
      if (paneExag) paneExag.textContent = `${vp.settings.exag.toFixed(1)}×`;
    }
  });
  settingsContent.addEventListener('click', (e) => {
    const camMode = e.target.closest('[data-camera-mode]');
    if (camMode) {
      const vp = activeVp();
      vp.mode = camMode.dataset.cameraMode;
      renderAll();
      return;
    }
    const camAction = e.target.closest('[data-camera-action]');
    if (camAction) {
      const vp = activeVp();
      vp.mode = '轨道相机';
      vp.settings.projection = '透视';
      vp.settings.fov = 60;
      vp.camera = { scale: 1, rot: 0 };
      vp.coord = { lon: 121.4737, lat: 31.2304, h: 8.42 };
      renderAll();
      toast(`${vp.name} 相机已重置`);
      return;
    }
    const globeToggle = e.target.closest('[data-globe-toggle]');
    if (globeToggle) {
      const vp = activeVp();
      vp.settings.grid = !vp.settings.grid;
      renderAll();
      return;
    }
    const render = e.target.closest('[data-render-opt]');
    if (render) {
      activeVp().settings.render = render.dataset.renderOpt;
      renderPanes(); renderSettings();
      return;
    }
    const toggle = e.target.closest('[data-toggle-setting]');
    if (toggle) {
      const key = toggle.dataset.toggleSetting;
      const vp = activeVp();
      vp.settings[key] = !vp.settings[key];
      renderPanes(); renderSettings();
      return;
    }
    const layerToggle = e.target.closest('[data-layer-toggle]');
    if (layerToggle) {
      const vp = activeVp();
      const layer = layerOf(vp, state.selected[vp.id]);
      if (layer) {
        const key = layerToggle.dataset.layerToggle;
        layer[key] = !layer[key];
        renderAll();
      }
      return;
    }
    const layerColor = e.target.closest('[data-layer-color]');
    if (layerColor) {
      const vp = activeVp();
      const layer = layerOf(vp, state.selected[vp.id]);
      if (layer) {
        layer.colorMode = layerColor.dataset.layerColor;
        renderSettings();
      }
      return;
    }
    const layerAction = e.target.closest('[data-layer-action]');
    if (layerAction) {
      const vp = activeVp();
      const layer = layerOf(vp, state.selected[vp.id]);
      if (!layer || layer.type === 'globe') return;
      const action = layerAction.dataset.layerAction;
      if (action === 'open-script' || action === 'run-script') {
        $('#scriptFileName').textContent = layer.meta?.file || 'satellite.gd';
        $('#scriptCode').textContent = layer.scriptCode || '';
        updateScriptLines(false);
        state.panels.script.open = true;
        renderAll();
        if (action === 'run-script') $('#runScript').click();
      } else if (action === 'reload') {
        layer.state = 'loading';
        layer.progress = 0;
        layer.sub = '重新加载 · 0%';
        renderAll();
        setTimeout(() => {
          if (!layerOf(vp, layer.key)) return;
          layer.state = 'ready';
          layer.progress = 100;
          layer.sub = '就绪';
          renderAll();
        }, 900);
      } else if (action === 'pause') {
        layer.state = layer.state === 'paused' ? 'ready' : 'paused';
        layer.sub = layer.state === 'paused' ? '已暂停' : '就绪';
        renderAll();
      } else if (action === 'remove') {
        vp.layers = vp.layers.filter((l) => l.key !== layer.key);
        state.selected[vp.id] = null;
        renderAll();
        toast(`${layer.name} 已从 ${vp.name} 移除`);
      }
      return;
    }
  });

  settingsContent.addEventListener('input', (e) => {
    const el = e.target.closest('[data-layer-set]');
    if (!el) return;
    const vp = activeVp();
    const layer = layerOf(vp, state.selected[vp.id]);
    if (!layer) return;
    const key = el.dataset.layerSet;
    const num = Number(el.value);
    layer[key] = key === 'opacity' ? num / 100 : num;
    const pill = el.previousElementSibling && el.previousElementSibling.querySelector('.value-pill');
    if (pill) {
      if (key === 'opacity') pill.textContent = `${Math.round(num)}%`;
      else if (key === 'pointSize') pill.textContent = `${num.toFixed(1)} px`;
      else if (key === 'brightness' || key === 'contrast') pill.textContent = `${num.toFixed(2)}×`;
      else if (key === 'exag') pill.textContent = `${num.toFixed(1)}×`;
    }
  });

  settingsContent.addEventListener('change', (e) => {
    const el = e.target.closest('[data-layer-set]');
    if (!el) return;
    const vp = activeVp();
    const layer = layerOf(vp, state.selected[vp.id]);
    if (!layer) return;
    if (el.tagName === 'SELECT') {
      layer[el.dataset.layerSet] = el.value;
    }
    renderSettings();
  });

  /* -------- topbar overlays -------- */
  function openModal(name) {
    $$('.modal').forEach((m) => m.classList.toggle('is-open', m.dataset.modal === name));
  }
  function closeModal(name) {
    const modal = $(`.modal[data-modal="${name}"]`);
    if (modal) modal.classList.remove('is-open');
  }
  function openFly() {
    const vp = activeVp();
    $('#flyLon').value = vp.coord.lon.toFixed(4);
    $('#flyLat').value = vp.coord.lat.toFixed(4);
    $('#flyHeight').value = vp.coord.h.toFixed(2);
    $('#flyCurrent').textContent = `${Math.abs(vp.coord.lon).toFixed(4)}°E, ${Math.abs(vp.coord.lat).toFixed(4)}°N · ${vp.coord.h.toFixed(2)} km`;
    openModal('fly');
  }
  function openModelModal() {
    $('#modelForm').reset();
    $('#modelProvider').value = 'openai';
    $('#modelBase').value = 'https://api.openai.com/v1';
    $('#modelId').value = 'gpt-4o';
    $('#modelContext').value = '128K';
    openModal('model');
    setTimeout(() => $('#modelName').focus(), 30);
  }
  function openPanel(name) {
    state.panels[name].open = true;
    renderAll();
    if (name === 'ai') $('#aiInput').focus();
    if (name === 'script') $('#scriptCode').focus();
  }
  function closePanel(name) {
    const st = state.panels[name];
    if (name === 'script' && state.scriptRunning) {
      state.scriptRunning = false;
      $('#scriptLog').textContent += '\n> 面板关闭，脚本已停止';
    }
    if (name === 'script') {
      state.scriptAiDraft = false;
      scriptAiBanner.hidden = true;
    }
    st.open = false;
    st.mode = 'docked';
    if (name === 'ai') closeModelPop();
    renderAll();
  }
  function togglePanel(name) {
    if (state.panels[name].open) closePanel(name);
    else openPanel(name);
  }
  function detachPanel(name) {
    if (state.platform === 'web') { toast('Web 端不支持面板独立窗口'); return; }
    state.panels[name].open = true;
    state.panels[name].mode = 'floating';
    renderAll();
  }
  function dockPanel(name) {
    state.panels[name].open = true;
    state.panels[name].mode = 'docked';
    renderAll();
  }
  function openAI() { togglePanel('ai'); }
  function closeAI() { closePanel('ai'); }
  function openScript() { togglePanel('script'); }
  function closeScript() { closePanel('script'); }
  function addAIMessage(role, text) {
    const log = $('#aiLog');
    const mine = role === 'user';
    const el = document.createElement('div');
    el.className = `multi-ai-msg ${mine ? 'user' : 'assistant'}`;
    el.innerHTML = `<b>${mine ? '你' : 'AI 助手'}</b><p>${esc(text)}</p>`;
    log.appendChild(el);
    log.scrollTop = log.scrollHeight;
  }

  function addAIScriptCard(request) {
    const log = $('#aiLog');
    const code = `extends Node

func _ready() -> void:
    # AI 草案：需审查后运行
    for vp in viewer.viewports():
        vp.set_grid(false)`;
    const el = document.createElement('div');
    el.className = 'ai-script-card';
    el.dataset.aiScriptCard = '';
    el.innerHTML = `<div class="ai-script-head"><svg class="ic"><use href="#i-terminal"/></svg><b>GDScript 草案</b><span>待审查</span></div>
      <p>${esc(request)}</p>
      <pre class="ai-script-code">${esc(code)}</pre>
      <div class="ai-script-meta"><span>编译通过</span><span>场景/相机 API</span><span>检查点 #12</span></div>
      <div class="ai-script-actions">
        <button type="button" data-ai-script="open">查看脚本</button>
        <button type="button" class="primary" data-ai-script="run">审查并运行</button>
        <button type="button" data-ai-script="dismiss">忽略</button>
      </div>`;
    log.appendChild(el);
    log.scrollTop = log.scrollHeight;
  }

  /* -------- main click routing -------- */
  document.addEventListener('click', (e) => {
    if (!e.target.closest('#aiModelPop') && !e.target.closest('#aiModelButton')) closeModelPop();
    if (!e.target.closest('#panePop') && !e.target.closest('[data-pane-tool]') && !e.target.closest('[data-pane-menu]')) {
      closePanePop();
    }
    if (!e.target.closest('.pop-wrap')) {
      $$('.pop-wrap.open').forEach((w) => w.classList.remove('open'));
    }
    const menuAct = e.target.closest('[data-menu-act]');
    if (menuAct) {
      const act = menuAct.dataset.menuAct;
      const id = state.menuId;
      hidePaneMenu();
      if (!id || !viewports.has(id)) return;
      if (act === 'fly') { setActive(id); openFly(); return; }
      if (act === 'viewpoints' || act === 'display') {
        setActive(id);
        const anchor = document.querySelector(`.vp-pane[data-vp="${id}"] [data-pane-menu]`) || document.querySelector(`.vp-pane[data-vp="${id}"]`);
        openPanePop(act, id, anchor);
        return;
      }
      if (act === 'grid') {
        const vp = viewports.get(id);
        vp.settings.grid = !vp.settings.grid;
        state.active = id;
        renderAll();
        return;
      }
      if (act === 'split-h') { splitLeaf(id, 'h'); return; }
      if (act === 'split-v') { splitLeaf(id, 'v'); return; }
      if (act === 'detach') { setActive(id); detach(id); return; }
      if (act === 'max') { setActive(id); toggleMax(); return; }
      if (act === 'dock') { dock(id); return; }
      if (act === 'close') { closeLeaf(id); return; }
    }

    const appMenuButton = e.target.closest('[data-app-menu]');
    if (appMenuButton) {
      e.stopPropagation();
      const wrap = appMenuButton.closest('.pop-wrap');
      const willOpen = !wrap.classList.contains('open');
      $$('.pop-wrap.open').forEach((w) => w.classList.remove('open'));
      wrap.classList.toggle('open', willOpen);
      appMenuButton.setAttribute('aria-expanded', String(willOpen));
      return;
    }
    const appItem = e.target.closest('[data-app]');
    if (appItem) {
      e.stopPropagation();
      const name = appItem.dataset.app;
      $$('.pop-wrap.open').forEach((w) => w.classList.remove('open'));
      if (name === 'settings') openModal('settings');
      else if (name === 'about') openModal('about');
      else if (name === 'quit') toast('退出：先取消任务，再释放图层与 GPU（会话不保存）');
      return;
    }
    const paneTool = e.target.closest('[data-pane-tool]');
    if (paneTool) {
      e.stopPropagation();
      const id = Number(paneTool.dataset.vp);
      const kind = paneTool.dataset.paneTool;
      if (!viewports.has(id)) return;
      if (kind === 'fly') { setActive(id); openFly(); return; }
      if (kind === 'viewpoints' || kind === 'display') { openPanePop(kind, id, paneTool); return; }
      if (kind === 'grid') {
        const vp = viewports.get(id);
        vp.settings.grid = !vp.settings.grid;
        state.active = id;
        renderAll();
        return;
      }
      return;
    }
    const paneRender = e.target.closest('[data-pane-render]');
    if (paneRender) {
      const vp = activeVp();
      vp.settings.render = paneRender.dataset.paneRender;
      renderAll();
      return;
    }
    const paneToggle = e.target.closest('[data-pane-toggle]');
    if (paneToggle) {
      const vp = activeVp();
      const key = paneToggle.dataset.paneToggle;
      vp.settings[key] = !vp.settings[key];
      renderAll();
      return;
    }
    const paneViewpoint = e.target.closest('[data-pane-viewpoint]');
    if (paneViewpoint) {
      const p = viewpoints[Number(paneViewpoint.dataset.paneViewpoint)];
      const vp = activeVp();
      if (p) {
        vp.coord = { lon: p.lon, lat: p.lat, h: p.h };
        closePanePop();
        renderAll();
        toast(`${vp.name} 飞往视点：${p.name}`);
      }
      return;
    }
    const paneSaveVp = e.target.closest('[data-pane-save-vp]');
    if (paneSaveVp) {
      const vp = activeVp();
      viewpoints.push({ name: `视点 ${viewpoints.length + 1}`, lon: vp.coord.lon, lat: vp.coord.lat, h: vp.coord.h });
      renderAll();
      toast('已保存当前视点');
      return;
    }
    const paneClose = e.target.closest('[data-pane-close]');
    if (paneClose) { closePanePop(); return; }

    const panelAction = e.target.closest('[data-panel-action]');
    if (panelAction) {
      e.stopPropagation();
      const action = panelAction.dataset.panelAction;
      if (action.startsWith('close-')) { closePanel(action.slice(6)); return; }
      if (action.startsWith('detach-')) { detachPanel(action.slice(7)); return; }
      if (action.startsWith('dock-')) { dockPanel(action.slice(5)); return; }
    }

    const aiPrompt = e.target.closest('[data-ai-prompt]');
    if (aiPrompt) {
      const input = $('#aiInput');
      input.value = aiPrompt.dataset.aiPrompt;
      input.dispatchEvent(new Event('input', { bubbles: true }));
      input.focus();
      return;
    }

    const aiModelBtn = e.target.closest('#aiModelButton');
    if (aiModelBtn) {
      e.stopPropagation();
      if (aiModelPop.hidden) openModelPop(); else closeModelPop();
      return;
    }
    const modelPick = e.target.closest('[data-model-pick]');
    if (modelPick) {
      e.stopPropagation();
      state.activeModel = modelPick.dataset.modelPick;
      renderAIModels();
      closeModelPop();
      const picked = state.aiModels.find((m) => m.id === state.activeModel);
      if (picked) toast(`已选择模型：${picked.name}`);
      return;
    }
    const aiAddModel = e.target.closest('[data-ai-add-model]');
    if (aiAddModel) {
      e.stopPropagation();
      closeModelPop();
      openModelModal();
      return;
    }

    const aiScriptAction = e.target.closest('[data-ai-script]');
    if (aiScriptAction) {
      e.stopPropagation();
      const card = aiScriptAction.closest('[data-ai-script-card]');
      const action = aiScriptAction.dataset.aiScript;
      if (action === 'dismiss') { card.remove(); return; }
      const code = card.querySelector('.ai-script-code').textContent;
      $('#scriptCode').textContent = code;
      updateScriptLines(true);
      state.scriptAiDraft = true;
      state.panels.script.open = true;
      state.panels.script.mode = 'docked';
      scriptAiBanner.hidden = false;
      renderAll();
      toast(action === 'run' ? '脚本草案已打开：请审查、确认信任并手动运行' : 'AI 脚本草案已放入脚本面板');
      return;
    }

    const scriptAct = e.target.closest('[data-script-act]');
    if (scriptAct) {
      e.stopPropagation();
      const act = scriptAct.dataset.scriptAct;
      if (act === 'save') { saveScript(); return; }
      if (act === 'new') { newScript(); return; }
      if (act === 'open') { toast('打开脚本：原型选择 user://scripts/*.gd'); return; }
    }

    const act = e.target.closest('[data-act]');
    if (act) {
      const name = act.dataset.act;
      if (name === 'props') { togglePanel('props'); return; }
      if (name === 'ai') { openAI(); return; }
      if (name === 'script') { openScript(); return; }
      if (name === 'ai-attach') { toast('附件与 @ 引用入口（原型）'); return; }
      if (name === 'ai-mode') {
        const modes = ['脚本修改', '仅建议', '只读'];
        state.aiModeIndex = (state.aiModeIndex + 1) % modes.length;
        $('#aiModeLabel').textContent = modes[state.aiModeIndex];
        toast(`AI 修改范围：${modes[state.aiModeIndex]}`);
        return;
      }
      if (name === 'close-ai') { closeAI(); return; }
      if (name === 'close-script') { closeScript(); return; }
      if (name === 'close-props') { closePanel('props'); return; }
      if (name === 'add-model') { openModelModal(); return; }
      if (name === 'show-script-diff') { toast('脚本差异 +9 −0 · 仅场景/相机 API'); return; }
      if (name === 'copy-mcp') { toast('已复制 MCP 端点：127.0.0.1:8731/mcp'); return; }
      if (name === 'settings') { openModal('settings'); return; }
      if (name === 'use-current') {
        const vp = activeVp();
        $('#flyLon').value = vp.coord.lon.toFixed(4);
        $('#flyLat').value = vp.coord.lat.toFixed(4);
        $('#flyHeight').value = vp.coord.h.toFixed(2);
        return;
      }
      if (name === 'reset-vp') {
        activeVp().settings = { ...DEFAULT_SETTINGS };
        renderAll();
        toast(`${activeVp().name} 已恢复默认设置`);
        return;
      }
      if (name === 'do-add') { doAdd(); return; }
    }

    const pick = e.target.closest('[data-pick]');
    if (pick) {
      setActive(Number(pick.dataset.pick));
      vpPicker.hidden = true;
      return;
    }

    const treeToggle = e.target.closest('[data-tree-toggle]');
    if (treeToggle) {
      e.stopPropagation();
      const id = Number(treeToggle.dataset.treeToggle);
      const vp = viewports.get(id);
      if (vp) {
        vp.treeOpen = !vp.treeOpen;
        state.active = id;
        renderAll();
      }
      return;
    }
    const paneAdd = e.target.closest('[data-pane-add]');
    if (paneAdd) {
      e.stopPropagation();
      setActive(Number(paneAdd.dataset.paneAdd));
      openAdd();
      return;
    }
    const cameraNode = e.target.closest('[data-camera]');
    if (cameraNode) {
      e.stopPropagation();
      const id = Number(cameraNode.dataset.camera);
      if (viewports.has(id)) {
        state.active = id;
        state.selected[id] = 'camera';
        renderAll();
      }
      return;
    }
    const treeNode = e.target.closest('[data-tree-node]');
    if (treeNode) {
      e.stopPropagation();
      const vp = viewports.get(Number(treeNode.dataset.vp));
      const key = treeNode.dataset.treeNode;
      if (vp) {
        if (vp.collapsed.has(key)) vp.collapsed.delete(key);
        else vp.collapsed.add(key);
        state.active = vp.id;
        renderAll();
      }
      return;
    }

    const menuButton = e.target.closest('[data-pane-menu]');
    if (menuButton) {
      e.stopPropagation();
      openPaneMenu(menuButton, Number(menuButton.dataset.paneMenu));
      return;
    }

    const paneAct = e.target.closest('[data-pane-act]');
    if (paneAct) {
      e.stopPropagation();
      const id = Number(paneAct.dataset.vp);
      const action = paneAct.dataset.paneAct;
      if (action === 'split-h') { setActive(id); splitLeaf(id, 'h'); }
      else if (action === 'split-v') { setActive(id); splitLeaf(id, 'v'); }
      else if (action === 'detach') { setActive(id); detach(id); }
      else if (action === 'dock') { dock(id); }
      else if (action === 'max') { setActive(id); toggleMax(); }
      else if (action === 'close') { closeLeaf(id); }
      return;
    }

    const detachedCard = e.target.closest('[data-detached-id]');
    if (detachedCard) {
      setActive(Number(detachedCard.dataset.detachedId), true);
      return;
    }

    const eye = e.target.closest('[data-eye]');
    if (eye) {
      e.stopPropagation();
      const row = eye.closest('.layer-row');
      const vp = row ? viewports.get(Number(row.dataset.vp)) : null;
      const layer = vp ? layerOf(vp, eye.dataset.eye) : null;
      if (layer) layer.visible = layer.visible === false;
      renderAll();
      return;
    }

    const layerRow = e.target.closest('.pane-tree .layer-row');
    if (layerRow) {
      e.stopPropagation();
      const vpId = Number(layerRow.dataset.vp);
      if (viewports.has(vpId)) {
        state.active = vpId;
        state.selected[vpId] = layerRow.dataset.layer;
        renderAll();
      }
      return;
    }

    const pane = e.target.closest('.vp-pane');
    if (pane && !e.target.closest('.vp-header')) {
      setActive(Number(pane.dataset.vp), true);
      return;
    }
    const header = e.target.closest('.vp-header');
    if (header) { setActive(Number(header.dataset.vp), true); return; }

    if (!e.target.closest('#vpPicker') && !e.target.closest('[data-act="vp-picker"]')) {
      vpPicker.hidden = true;
      hidePaneMenu();
    }
  });
  grid.addEventListener('dragstart', (e) => {
    const row = e.target.closest('.pane-tree .layer-row');
    if (!row) return;
    state.drag = { from: Number(row.dataset.vp), key: row.dataset.layer };
    row.classList.add('dragging');
    e.dataTransfer.effectAllowed = 'copy';
    e.dataTransfer.setData('text/plain', row.dataset.layer);
  });
  grid.addEventListener('dragend', () => {
    state.drag = null;
    $$('.vp-pane.drag-over').forEach((p) => p.classList.remove('drag-over'));
    $$('.layer-row.dragging').forEach((r) => r.classList.remove('dragging'));
  });
  grid.addEventListener('dragover', (e) => {
    const pane = e.target.closest('.vp-pane');
    if (!pane || !state.drag) return;
    if (Number(pane.dataset.vp) === state.drag.from) return;
    e.preventDefault();
    e.dataTransfer.dropEffect = 'copy';
    pane.classList.add('drag-over');
  });
  grid.addEventListener('dragleave', (e) => {
    const pane = e.target.closest('.vp-pane');
    if (pane) pane.classList.remove('drag-over');
  });
  grid.addEventListener('drop', (e) => {
    const pane = e.target.closest('.vp-pane');
    if (!pane || !state.drag) return;
    e.preventDefault();
    pane.classList.remove('drag-over');
    const targetId = Number(pane.dataset.vp);
    if (targetId === state.drag.from) return;
    const target = viewports.get(targetId);
    const source = viewports.get(state.drag.from);
    const key = state.drag.key;
    const sourceLayer = layerOf(source, key);
    if (!target || !sourceLayer) return;
    if (layerOf(target, key)) { toast(`${target.name} 已包含该共享数据`); return; }
    target.layers.push({ ...sourceLayer, visible: true, state: 'ready', progress: 100, sub: '共享数据 · 独立显示' });
    state.selected[target.id] = key;
    renderAll();
    toast(`${sourceLayer.name} 已共享到 ${target.name} · 引用 ${sharedCount(key)}`);
  });

  /* -------- add data -------- */
  function openAdd() {
    const vp = activeVp();
    if (!vp.layers.length) state.addSource = 'ellipsoid';
    else if (vp.layers.some((l) => l.type === 'globe') && state.addSource === 'ellipsoid') state.addSource = 'local';
    $('#addTarget').textContent = vp.name;
    $('#addName').value = state.addSource === 'ellipsoid' ? 'WGS84 椭球' : state.addSource === 'script' ? '脚本行为' : '新建数据';
    $('#modalLayer .modal[data-modal="add"]').classList.add('is-open');
    $$('#sourceGrid .source-card').forEach((b) => b.classList.toggle('is-on', b.dataset.source === state.addSource));
  }
  function closeAdd() { $('#modalLayer .modal[data-modal="add"]').classList.remove('is-open'); }
  function doAdd() {
    const vp = activeVp();
    if (state.addSource === 'ellipsoid') {
      if (vp.layers.some((l) => l.type === 'globe')) {
        toast(`${vp.name} 已包含 WGS84 椭球`);
        return;
      }
      const name = ($('#addName').value || '').trim() || 'WGS84 椭球';
      const layer = globeLayer();
      layer.name = name;
      vp.layers.unshift(layer);
      state.selected[vp.id] = 'globe';
      closeAdd();
      renderAll();
      toast(`WGS84 椭球已添加到 ${vp.name}`);
      return;
    }
    if (state.addSource === 'script') {
      const name = ($('#addName').value || '').trim() || '脚本行为';
      const key = `script-${state.addCounter++}`;
      const code = `extends Node

func _ready() -> void:
    print("${name} ready")`;
      vp.layers.push({
        key, type: 'script', name, sub: 'GDScript · 待运行', visible: true, opacity: 1,
        state: 'ready', scriptCode: code,
        meta: { file: `${key}.gd`, entry: 'run(viewer)', permissions: '应用级' }
      });
      state.selected[vp.id] = key;
      $('#scriptFileName').textContent = `${key}.gd`;
      $('#scriptCode').textContent = code;
      updateScriptLines(true);
      state.panels.script.open = true;
      closeAdd();
      renderAll();
      toast(`脚本已添加到 ${vp.name}`);
      return;
    }
    const name = ($('#addName').value || '').trim() || `新建数据 ${state.addCounter}`;
    const key = `new-${state.addCounter++}`;
    vp.layers.push({
      key, type: 'tiles', name, sub: '排队 · 0%', visible: true, opacity: 1,
      state: 'loading', progress: 0, outline: false,
      meta: { format: '3D Tiles 1.1', tiles: '0 / —', geometricError: '—', sse: '16 px', memory: '—' }
    });
    state.selected[vp.id] = key;
    closeAdd();
    renderAll();
    toast(`${name} 已添加到 ${vp.name} · 独立数据`);
    setTimeout(() => {
      const layer = layerOf(vp, key);
      if (!layer) return;
      layer.state = 'ready';
      layer.progress = 100;
      layer.sub = '就绪 · 32 MB';
      renderAll();
    }, 1200);
  }

  document.addEventListener('click', (e) => {
    const source = e.target.closest('#sourceGrid [data-source]');
    if (source) {
      state.addSource = source.dataset.source;
      $$('#sourceGrid .source-card').forEach((b) => b.classList.toggle('is-on', b === source));
      $('#addName').value = state.addSource === 'ellipsoid' ? 'WGS84 椭球' : state.addSource === 'script' ? '脚本行为' : '新建数据';
      return;
    }
    const close = e.target.closest('[data-close]');
    if (close) { closeModal(close.dataset.close); return; }
    if (e.target.classList.contains('modal-scrim')) {
      const modal = e.target.closest('.modal');
      if (modal) closeModal(modal.dataset.modal);
    }
  });

  $('#flyForm').addEventListener('submit', (e) => {
    e.preventDefault();
    const vp = activeVp();
    vp.coord = {
      lon: Number($('#flyLon').value) || 0,
      lat: Number($('#flyLat').value) || 0,
      h: Number($('#flyHeight').value) || 0
    };
    renderPanes();
    renderTopbar();
    closeModal('fly');
    toast(`${vp.name} 已飞往目标坐标`);
  });

  $('#modelForm').addEventListener('submit', (e) => {
    e.preventDefault();
    const name = $('#modelName').value.trim();
    const model = $('#modelId').value.trim();
    const base = $('#modelBase').value.trim();
    if (!name || !model || !base) return;
    const provider = $('#modelProvider').value;
    const providerLabel = provider === 'openai' ? 'OpenAI 兼容' : provider === 'ollama' ? 'Ollama' : '自定义';
    const id = `m${state.modelSeq++}`;
    state.aiModels.push({
      id, name, provider: providerLabel, model, base,
      key: $('#modelKey').value,
      context: $('#modelContext').value.trim() || '—'
    });
    state.activeModel = id;
    closeModal('model');
    closeModelPop();
    renderAIModels();
    toast(`模型已添加并选择：${name}`);
  });

  $('#modelProvider').addEventListener('change', (e) => {
    const v = e.target.value;
    if (v === 'ollama') {
      $('#modelBase').value = 'http://localhost:11434/v1';
      $('#modelId').value = 'llama3.1';
      $('#modelContext').value = '32K';
    } else if (v === 'openai') {
      $('#modelBase').value = 'https://api.openai.com/v1';
      $('#modelId').value = 'gpt-4o';
      $('#modelContext').value = '128K';
    } else {
      $('#modelBase').value = '';
      $('#modelId').value = '';
      $('#modelContext').value = '';
    }
  });

  const aiInput = $('#aiInput');
  const aiSend = $('.ai-send');
  function syncComposer() {
    aiInput.style.height = 'auto';
    aiInput.style.height = `${Math.min(aiInput.scrollHeight, 96)}px`;
    aiSend.disabled = !aiInput.value.trim();
  }
  aiInput.addEventListener('input', syncComposer);
  aiInput.addEventListener('keydown', (e) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      $('#aiForm').requestSubmit();
    }
  });
  syncComposer();

  $('#aiForm').addEventListener('submit', (e) => {
    e.preventDefault();
    const input = $('#aiInput');
    const text = input.value.trim();
    if (!text) return;
    addAIMessage('user', text);
    input.value = '';
    syncComposer();
    const model = state.aiModels.find((m) => m.id === state.activeModel) || state.aiModels[0];
    const modelNote = model && model.id !== 'local' ? `当前模型：${model.name}。` : '当前为本地规则演示。';
    setTimeout(() => {
      addAIMessage('assistant', `已生成 GDScript 草案。${modelNote}我不会读写 JSON 场景数据，也不会自动运行；请在脚本面板审查、确认信任后手动运行。`);
      addAIScriptCard(text);
    }, 400);
  });

  function updateScriptLines(dirty = true) {
    const lines = scriptCode.innerText.replace(/\n$/, '').split('\n');
    scriptLines.textContent = lines.map((_, i) => i + 1).join('\n');
    if (dirty) scriptDirty.textContent = '未保存';
  }
  function saveScript() {
    scriptDirty.textContent = '已保存';
    toast('脚本已保存到 user://scripts/satellite.gd');
  }
  function newScript() {
    scriptCode.textContent = 'extends Node\n\nfunc _ready() -> void:\n    print("DigitalViewer script ready")';
    updateScriptLines(true);
    scriptCode.focus();
    toast('已新建脚本');
  }
  scriptCode.addEventListener('input', () => updateScriptLines(true));
  scriptCode.addEventListener('keydown', (e) => {
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 's') {
      e.preventDefault();
      saveScript();
    }
    if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
      e.preventDefault();
      $('#runScript').click();
    }
  });
  updateScriptLines(false);

  function executeScript() {
    state.scriptRunning = true;
    const log = $('#scriptLog');
    log.textContent = '> run satellite.gd\n> scope = application\n> 执行完成 · 0 errors';
    renderTargets();
  }
  $('#runScript').addEventListener('click', () => {
    $('#runTrust').checked = false;
    $('#confirmRun').disabled = true;
    openModal('run-script');
  });
  $('#runTrust').addEventListener('change', (e) => {
    $('#confirmRun').disabled = !e.target.checked;
  });
  $('#confirmRun').addEventListener('click', () => {
    if (!$('#runTrust').checked) return;
    closeModal('run-script');
    executeScript();
  });
  $('#stopScript').addEventListener('click', () => {
    state.scriptRunning = false;
    $('#scriptLog').textContent += '\n> 已停止';
    renderTargets();
  });

  /* -------- splitters -------- */
  let splitDrag = null;
  grid.addEventListener('pointerdown', (e) => {
    const divider = e.target.closest('.split-divider');
    if (!divider) return;
    e.preventDefault();
    const nodeEl = divider.closest('.split-node');
    const split = findSplit(tree, nodeEl.dataset.node);
    if (!split) return;
    divider.classList.add('dragging');
    splitDrag = { split, nodeEl, rect: nodeEl.getBoundingClientRect(), divider };
    document.body.style.cursor = split.orientation === 'h' ? 'col-resize' : 'row-resize';
  });
  window.addEventListener('pointermove', (e) => {
    if (!splitDrag) return;
    const { split, nodeEl, rect } = splitDrag;
    const ratio = split.orientation === 'h'
      ? (e.clientX - rect.left) / rect.width
      : (e.clientY - rect.top) / rect.height;
    split.ratio = clamp(ratio, 0.02, 0.98);
    nodeEl.style.setProperty('--ratio', split.ratio);
  });
  window.addEventListener('pointerup', () => {
    if (!splitDrag) return;
    splitDrag.divider.classList.remove('dragging');
    splitDrag = null;
    document.body.style.cursor = '';
  });

  /* -------- flexible panels: resize / detach window -------- */
  let sideDrag = null;
  let stackDrag = null;
  let scriptDrag = null;
  let floatDrag = null;
  let floatResize = null;

  sideResizer.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    sideDrag = { x: e.clientX, w: sideDock.getBoundingClientRect().width };
    document.body.style.cursor = 'col-resize';
  });
  stackResizer.addEventListener('pointerdown', (e) => {
    e.preventDefault();
    stackDrag = { y: e.clientY, h: sideDock.getBoundingClientRect().height };
    document.body.style.cursor = 'row-resize';
  });
  $('.script-slot .script-resizer').addEventListener('pointerdown', (e) => {
    if (state.panels.script.mode !== 'docked') return;
    e.preventDefault();
    scriptDrag = { y: e.clientY, h: scriptSlot.getBoundingClientRect().height };
    document.body.style.cursor = 'row-resize';
  });
  floatLayer.addEventListener('pointerdown', (e) => {
    const panel = e.target.closest('.side-panel.is-floating');
    if (!panel) return;
    panel.style.zIndex = String(++state.floatZ);
    if (e.target.closest('button')) return;
    const grip = e.target.closest('[data-float-resize]');
    const r = panel.getBoundingClientRect();
    if (grip) {
      e.preventDefault();
      floatResize = { panel, x: e.clientX, y: e.clientY, w: r.width, h: r.height };
      document.body.style.cursor = 'nwse-resize';
      return;
    }
    if (e.target.closest('.panel-head, .panel-bar, .script-toolbar')) {
      floatDrag = { panel, x: e.clientX, y: e.clientY, left: r.left, top: r.top };
      document.body.style.cursor = 'move';
    }
  });
  window.addEventListener('pointermove', (e) => {
    if (sideDrag) {
      const w = clamp(sideDrag.w - (e.clientX - sideDrag.x), 300, Math.min(520, window.innerWidth - 520));
      app.style.setProperty('--side-w', `${w}px`);
      return;
    }
    if (stackDrag) {
      const ratio = clamp((e.clientY - sideDock.getBoundingClientRect().top) / Math.max(1, stackDrag.h), 0.28, 0.72);
      state.stackRatio = ratio;
      propSlot.style.flex = String(ratio);
      aiSlot.style.flex = String(1 - ratio);
      return;
    }
    if (scriptDrag) {
      const maxH = Math.max(240, mainStack.getBoundingClientRect().height - 160);
      const h = clamp(scriptDrag.h - (e.clientY - scriptDrag.y), 220, maxH);
      state.scriptHeight = h;
      scriptSlot.style.height = `${h}px`;
      return;
    }
    if (floatDrag) {
      const { panel } = floatDrag;
      const maxX = window.innerWidth - panel.offsetWidth - 8;
      const maxY = window.innerHeight - panel.offsetHeight - 8;
      panel.style.left = `${clamp(floatDrag.left + (e.clientX - floatDrag.x), 8, maxX)}px`;
      panel.style.top = `${clamp(floatDrag.top + (e.clientY - floatDrag.y), 60, maxY)}px`;
      return;
    }
    if (floatResize) {
      const { panel } = floatResize;
      const w = clamp(floatResize.w + (e.clientX - floatResize.x), 320, Math.min(900, window.innerWidth - 24));
      const h = clamp(floatResize.h + (e.clientY - floatResize.y), 260, Math.min(760, window.innerHeight - 70));
      panel.style.width = `${w}px`;
      panel.style.height = `${h}px`;
    }
  });
  window.addEventListener('pointerup', () => {
    if (!sideDrag && !stackDrag && !scriptDrag && !floatDrag && !floatResize) return;
    sideDrag = null;
    stackDrag = null;
    scriptDrag = null;
    floatDrag = null;
    floatResize = null;
    document.body.style.cursor = '';
  });

  /* -------- keyboard -------- */
  document.addEventListener('keydown', (e) => {
    if (e.key === 'Escape') {
      if (!paneMenu.hidden) { hidePaneMenu(); return; }
      if (state.panePopKind) { closePanePop(); return; }
      const floating = $$('.side-panel.is-floating').filter((panel) => !panel.hidden);
      if (floating.length) {
        const last = floating[floating.length - 1];
        const name = last.id === 'propPanel' ? 'props' : last.id === 'aiPanel' ? 'ai' : 'script';
        closePanel(name);
        return;
      }
      if (state.panels.ai.open) { closeAI(); return; }
      if (state.panels.script.open) { closeScript(); return; }
      const openModalEl = $('.modal.is-open');
      if (openModalEl) { closeModal(openModalEl.dataset.modal); return; }
      if (state.maximized) { state.maximized = false; renderPanes(); renderStatus(); return; }
      if (!vpPicker.hidden) { vpPicker.hidden = true; return; }
    }
    if (e.ctrlKey && !e.shiftKey && e.key === 'Tab') {
      e.preventDefault();
      const leaves = walkLeaves(tree);
      const index = leaves.findIndex((leaf) => leaf.id === state.active);
      setActive(leaves[(index + 1) % leaves.length].id);
      return;
    }
    if (e.ctrlKey && !e.shiftKey && e.key >= '1' && e.key <= '9') {
      e.preventDefault();
      const leaves = walkLeaves(tree);
      const leaf = leaves[Number(e.key) - 1];
      if (leaf) setActive(leaf.id);
      return;
    }
    if (e.ctrlKey && e.key === '\\') {
      e.preventDefault();
      splitLeaf(state.active, e.shiftKey ? 'v' : 'h');
    }
  });

  /* -------- demo / init -------- */
  function buildDemo(kind) {
    viewports.clear();
    state.detached.clear();
    state.selected = {};
    nextId = 1;
    splitSeq = 0;
    tree = null;
    const count = kind === '6' ? 6 : kind === '4' ? 4 : 2;
    const leaves = [];
    for (let i = 1; i <= count; i += 1) {
      const vp = createViewport(i <= 4 ? i : null);
      leaves.push(leafNode(vp.id));
    }
    const L = (i) => leaves[i - 1];
    if (kind === '2h') tree = splitNode('h', L(1), L(2), 0.5);
    else if (kind === '2v') tree = splitNode('v', L(1), L(2), 0.5);
    else if (kind === '4') {
      tree = splitNode('v',
        splitNode('h', L(1), L(2), 0.5),
        splitNode('h', L(3), L(4), 0.5),
        0.5);
    } else if (kind === '6') {
      tree = splitNode('v',
        splitNode('h', L(1), splitNode('h', L(2), L(3), 0.5), 0.333),
        splitNode('h', L(4), splitNode('h', L(5), L(6), 0.5), 0.333),
        0.5);
    } else tree = L(1);
    state.active = 1;
  }

  const params = new URLSearchParams(location.search);
  const platform = params.get('platform') === 'web' ? 'web' : 'desktop';
  state.platform = platform;
  app.dataset.platform = platform;
  const demo = params.get('demo');
  if (['2h', '2v', '4', '6'].includes(demo)) {
    buildDemo(demo);
  } else {
    const vp = createViewport(null);
    tree = leafNode(vp.id);
    state.active = vp.id;
  }
  const vpParam = Number(params.get('vp'));
  if (viewports.has(vpParam)) state.active = vpParam;
  const detachParam = Number(params.get('detach'));
  if (state.platform === 'desktop' && viewports.has(detachParam)) state.detached.add(detachParam);
  renderAll();
})();
