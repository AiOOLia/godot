/* ==========================================================================
   DigitalViewer UI Design v1 — 原型交互
   ========================================================================== */
(() => {
  'use strict';

  const $ = (sel, root = document) => root.querySelector(sel);
  const $$ = (sel, root = document) => Array.from(root.querySelectorAll(sel));
  const clamp = (v, a, b) => Math.min(b, Math.max(a, v));
  const esc = (s) => String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  const pad = (n) => String(n).padStart(2, '0');

  const state = {
    lang: 'zh',
    theme: 'dark',
    device: 'desktop',
    scene: 'main',
    selected: 'buildings',
    tab: 'source',
    aiTab: 'chat',
    right: 'props',
    diag: false,
    diagTab: 'overview',
    fullscreen: false,
    leftCollapsed: false,
    rightCollapsed: false,
    render: 'textured',
    grid: true,
    terrain: true,
    shadows: true,
    running: true,
    trusted: true,
    scriptTab: 'out',
    points: [
      { name: ['总览', 'Overview'], lon: 121.4737, lat: 31.2304, h: 8.42 },
      { name: ['陆家嘴', 'Lujiazui'], lon: 121.5057, lat: 31.2397, h: 1.8 }
    ],
    viewIndex: 3,
    coords: { lon: 121.4737, lat: 31.2304, h: 8.42, heading: 18, pitch: -35, distance: 12.4 },
    exag: 1.5,
    sceneVersion: 142,
    collapsedGroups: new Set(),
    chat: [
      {
        role: 'assistant',
        text: ['已连接当前场景。我通过 GDScript 与场景交互，草案需要你审查后运行。', 'Connected to the current scene. I interact through GDScript; drafts require your review before running.']
      },
      {
        role: 'user',
        text: ['用脚本隐藏经纬网，并飞往上海。', 'Use a script to hide the graticule and fly to Shanghai.']
      },
      {
        role: 'assistant',
        text: ['已生成 GDScript 草案，未自动执行。', 'GDScript draft generated; not run automatically.'],
        tools: [
          { name: 'script_inspect', summary: ['读取脚本会话', 'read script session'], args: '{ "path": "user://scripts" }' },
          { name: 'script_propose', summary: ['GDScript 草案', 'GDScript draft'], args: 'extends Node\n\nfunc _ready() -> void:\n    for vp in viewer.viewports():\n        vp.set_grid(false)' }
        ],
        note: ['草案已生成 · 待审查', 'Draft generated · review required']
      }
    ]
  };

  /* ------------------------------------------------------------------ 文案 */
  const T = {
    source: ['数据源', 'SOURCE'],
    display: ['显示', 'DISPLAY'],
    format: ['格式', 'Format'],
    crs: ['坐标系', 'CRS'],
    root: ['根资源', 'Root resource'],
    sourceLabel: ['来源', 'Source'],
    attribution: ['Attribution', 'Attribution'],
    status: ['状态', 'Status'],
    memory: ['Resident', 'Resident'],
    visible: ['图层可见', 'Layer visible'],
    shadows: ['投射阴影', 'Cast shadows'],
    opacity: ['透明度', 'Opacity'],
    order: ['图层顺序', 'Layer order'],
    orderHint: ['列表上方覆盖下方；仅影响影像与 DEM。', 'Upper layers cover lower ones. Applies to imagery and DEM only.'],
    moveUp: ['上移', 'Move up'],
    moveDown: ['下移', 'Move down'],
    terrain: ['地形', 'TERRAIN'],
    exag: ['地形夸张', 'Terrain exaggeration'],
    exagNum: ['数值输入 (0–100)', 'Numeric input (0–100)'],
    exagHint: ['应用在 DEM 合成后的地表高程，不改变三维模型锚点。', 'Applies to the composed surface elevation; model anchors are unchanged.'],
    animation: ['动画', 'ANIMATION'],
    play: ['播放', 'Play'],
    pause: ['暂停', 'Pause'],
    speed: ['速度', 'Speed'],
    focus: ['聚焦图层', 'Focus layer'],
    register: ['配准', 'REGISTRATION'],
    alignInfo: ['共享地理参考', 'Shared georeference'],
    alignInfoBody: ['该图层已加入共享 WGS84 / ECEF 原点与 ENU 基；无需单独配准。', 'This layer uses the shared WGS84 / ECEF origin and ENU basis. No per-layer registration is needed.'],
    forceEcef: ['强制按 ECEF 解释', 'Force ECEF interpretation'],
    forceEcefHint: ['仅在大型笛卡尔数据实际为 ECEF 时开启。', 'Enable only when large Cartesian data is actually ECEF.'],
    baseAlignBody: ['地球、影像与 DEM 共用同一 ECEF 原点与 ENU 基，不可单独配准。', 'Globe, imagery, and DEM share one ECEF origin and ENU basis. They cannot be registered individually.'],
    rasterCrs: ['栅格坐标系', 'Raster CRS'],
    rasterScheme: ['瓦片规则', 'Tile scheme'],
    bounds: ['范围', 'Bounds'],
    localMode: ['定位模式', 'Placement mode'],
    anchor: ['经纬高锚点', 'Lon / lat / height anchor'],
    localScene: ['局部场景', 'Local scene'],
    lon: ['经度', 'Longitude'],
    lat: ['纬度', 'Latitude'],
    height: ['高度 (m)', 'Height (m)'],
    enu: ['ENU 偏移 (m)', 'ENU offset (m)'],
    east: ['东', 'East'],
    north: ['北', 'North'],
    up: ['上', 'Up'],
    attitude: ['姿态', 'Orientation'],
    yaw: ['偏航', 'Yaw'],
    pitch: ['俯仰', 'Pitch'],
    roll: ['横滚', 'Roll'],
    origin: ['源原点', 'Source origin'],
    upAxis: ['上轴', 'Up axis'],
    scale: ['比例', 'Scale'],
    uniformScale: ['统一', 'Uniform'],
    perAxis: ['XYZ 分别', 'Per axis'],
    gizmo: ['在场景中调整', 'Adjust in scene'],
    apply: ['应用', 'Apply'],
    reset: ['重置', 'Reset'],
    pickTab: ['拾取', 'PICK'],
    pickEmpty: ['尚未拾取', 'Nothing picked'],
    pickEmptyBody: ['在视口中点击模型、地形或点云以查看属性。', 'Click a model, terrain, or point cloud in the viewport to inspect it.'],
    pickFly: ['飞往拾取点', 'Fly to pick'],
    copy: ['复制', 'Copy'],
    batch: ['Batch ID', 'Batch ID'],
    batchTable: ['Batch Table', 'Batch Table'],
    tile: ['Tile', 'Tile'],
    content: ['Content', 'Content'],
    surface: ['Surface', 'Surface'],
    triangle: ['Triangle / Point', 'Triangle / Point'],
    normal: ['法线', 'Normal'],
    distance: ['距离', 'Distance'],
    localPos: ['局部坐标', 'Local position'],
    ecef: ['ECEF', 'ECEF'],
    lonlat: ['经纬高', 'Lon / lat / height'],
    stats: ['统计', 'STATS'],
    pipeline: ['流送阶段', 'STREAM PIPELINE'],
    visibleTiles: ['可见瓦片', 'Visible tiles'],
    requests: ['请求中', 'In flight'],
    failed: ['失败', 'Failed'],
    resident: ['Resident', 'Resident'],
    cpu: ['CPU 内存', 'CPU memory'],
    gpu: ['GPU 内存', 'GPU memory'],
    textures: ['纹理', 'Textures'],
    queued: ['排队', 'Queued'],
    download: ['下载', 'Download'],
    parse: ['解析', 'Parse'],
    pack: ['打包', 'Pack'],
    upload: ['上传', 'Upload'],
    noAnim: ['无动画', 'No animation'],
    ready: ['就绪', 'Ready'],
    loading: ['加载中', 'Loading'],
    paused: ['已暂停', 'Paused'],
    failedState: ['失败', 'Failed'],
    warnState: ['部分失败', 'Partial failure'],
    idle: ['未加载', 'Not loaded'],
    sessionNote: ['本次会话 · 退出后清空', 'This session · cleared on exit'],
    addData: ['添加数据', 'Add data'],
    pickHint: ['在视口中点击可从场景拾取属性。', 'Click in the viewport to inspect scene properties.'],
    layerMenu: ['更多图层操作', 'More layer actions'],
    remove: ['移除图层', 'Remove layer'],
    reload: ['重载', 'Reload'],
    removeToast: ['已移除图层', 'Layer removed'],
    focusToast: ['正在聚焦图层', 'Focusing layer'],
    collapseGroup: ['折叠分组', 'Collapse group'],
    you: ['你', 'You'],
    aiName: ['DigitalViewer AI', 'DigitalViewer AI'],
    aiSuggest: ['隐藏经纬网', '飞往上海', '创建彩色圆环', '保存视点'],
    aiSuggestEn: ['Hide graticule', 'Fly to Shanghai', 'Create colored ring', 'Save viewpoint'],
    aiSuggestMobile: ['解释当前场景', '查看图层状态', '相机在哪里', '为什么不能修改'],
    aiSuggestMobileEn: ['Explain scene', 'Layer status', 'Camera position', 'Why read-only'],
    aiReply: ['已收到。我会生成 GDScript 草案；你审查并确认信任后，才能在脚本面板运行。', 'Received. I will generate a GDScript draft; you must review and confirm trust before it can run in the script panel.'],
    aiHideGrid: ['已生成隐藏经纬网的 GDScript 草案。', 'GDScript draft generated to hide the graticule.'],
    aiFly: ['已生成飞往上海的 GDScript 草案。', 'GDScript draft generated to fly to Shanghai.'],
    aiViewpoint: ['已生成保存视点的 GDScript 草案。', 'GDScript draft generated to save the viewpoint.'],
    aiRing: ['已生成创建 12 个球体的 GDScript 草案。', 'GDScript draft generated to create 12 spheres.'],
    aiAction: ['草案已生成 · 待审查', 'Draft generated · review required'],
    aiMobileReadonly: ['移动端为只读模式：我可以解释场景，但不会修改。脚本修改请在桌面/Web 审查并运行。', 'Mobile is read-only: I can explain the scene but will not modify it. Review and run script changes on desktop/web.'],
    aiTools: ['工具调用', 'Tool call'],
    diagTitle: ['状态与诊断', 'Status & diagnostics'],
    overview: ['概览', 'Overview'],
    errors: ['错误', 'Errors'],
    exportReport: ['导出脱敏报告', 'Export redacted report'],
    copy: ['复制', 'Copy'],
    clear: ['清空', 'Clear'],
    layer: ['图层', 'Layer'],
    stage: ['阶段', 'Stage'],
    waiting: ['等待', 'Waiting'],
    network: ['网络', 'Network'],
    warnings: ['条警告', 'warnings'],
    statusDefault: ['就绪', 'Ready'],
    diagNote: ['报告自动移除 Token、Authorization、Cookie 与敏感查询参数。', 'Reports automatically remove tokens, Authorization, cookies, and sensitive query values.'],
    errorCopy: ['已复制脱敏错误信息', 'Redacted error copied'],
    diagExport: ['已导出脱敏报告（示例）', 'Redacted report exported (sample)'],
    diagCleared: ['会话日志已清空', 'Session log cleared'],
    viewpointSaved: ['已保存当前视点', 'Current viewpoint saved'],
    viewpointDeleted: ['已删除视点', 'Viewpoint deleted'],
    flightStart: ['正在平滑飞往目标坐标', 'Flying smoothly to target'],
    flightCancel: ['已取消飞行', 'Flight cancelled'],
    copyCoord: ['坐标已复制', 'Coordinates copied'],
    copied: ['已复制', 'Copied'],
    resetView: ['已恢复默认视角', 'View reset'],
    zoom: ['视距已调整', 'Distance adjusted'],
    connectionOk: ['连接成功 · 模型响应 842 ms · 工具调用可用', 'Connected · model responded in 842 ms · tools available'],
    contextCleared: ['上下文已清空，场景与视点保留', 'Context cleared; scene and viewpoints kept'],
    trustRun: ['已信任并开始运行', 'Trusted and running'],
    scriptRun: ['脚本编译通过，run(viewer) 已执行', 'Script compiled; run(viewer) executed'],
    scriptStop: ['已停止脚本会话', 'Script session stopped'],
    scriptReset: ['场景已重置，AI 脚本桥恢复', 'Scene reset; AI script bridge restored'],
    addQueued: ['已加入队列', 'Queued'],
    addReady: ['已就绪', 'Ready'],
    filePicked: ['已选择示例文件', 'Sample file selected'],
    capsRead: ['Capabilities 已解析：18 个层级', 'Capabilities parsed: 18 levels'],
    added: ['已创建会话图层', 'Session layer created'],
    mobileSettings: ['设置以全屏页面呈现，与桌面共享语言与显示项。', 'Settings opens as a full-screen page sharing language and display options with desktop.'],
    about: ['DigitalViewer v1.0 · 设计评审原型', 'DigitalViewer v1.0 · Design review prototype'],
    quitDone: ['已取消任务、释放图层并清理会话（原型演示）', 'Tasks cancelled, layers released, and session cleaned (demo)'],
    noMore: ['原型未包含该后续页面', 'This prototype does not include that follow-up page']
  };
  const tx = (k) => (state.lang === 'en' ? T[k][1] : T[k][0]);
  const t = (pair) => (!pair ? '' : state.lang === 'en' ? pair[1] : pair[0]);
  const tt = (v) => (Array.isArray(v) ? t(v) : v);

  /* ------------------------------------------------------------------ 数据 */
  const GROUPS = [
    { id: 'base', name: ['地球与底图', 'Globe & basemap'] },
    { id: 'terrain', name: ['地形', 'Terrain'] },
    { id: 'data3d', name: ['三维数据', '3D data'] },
    { id: 'session', name: ['会话数据', 'Session data'] }
  ];

  const LAYERS = [
    {
      id: 'globe', group: 'base', type: 'globe',
      name: ['WGS84 地球', 'WGS84 globe'],
      sub: ['椭球 · 就绪', 'Ellipsoid · Ready'],
      kind: ['WGS84 椭球', 'WGS84 ellipsoid'],
      state: 'ready', visible: true, opacity: 100, shadows: false, geo: 'base',
      memory: 18, info: { format: ['椭球', 'Ellipsoid'], crs: 'WGS84 / ECEF', root: '—', source: ['内置', 'Built-in'], attribution: ['—', '—'] },
      stats: { visible: 0, resident: 0, requests: 0, failed: 0, cpu: 18, gpu: 2, textures: 0, queued: 0, download: 0, parse: 0, pack: 0, upload: 0 }
    },
    {
      id: 'imagery', group: 'base', type: 'imagery',
      name: ['全球影像底图', 'Global imagery'],
      sub: ['XYZ · 124 瓦片', 'XYZ · 124 tiles'],
      kind: ['XYZ 影像 · EPSG:3857', 'XYZ imagery · EPSG:3857'],
      state: 'ready', visible: true, opacity: 100, shadows: false, geo: 'raster', orderable: true,
      memory: 84, info: { format: ['XYZ / WebP', 'XYZ / WebP'], crs: 'EPSG:3857', root: '{z}/{x}/{y}.webp', source: ['示例影像服务', 'Sample imagery service'], attribution: ['© DigitalViewer 示例', '© DigitalViewer sample'] },
      stats: { visible: 124, resident: 318, requests: 4, failed: 0, cpu: 84, gpu: 96, textures: 124, queued: 0, download: 2, parse: 0, pack: 0, upload: 0 }
    },
    {
      id: 'night', group: 'base', type: 'imagery',
      name: ['夜间灯光影像', 'Night lights imagery'],
      sub: ['XYZ · 60% 不透明度', 'XYZ · 60% opacity'],
      kind: ['XYZ 影像 · EPSG:3857', 'XYZ imagery · EPSG:3857'],
      state: 'ready', visible: true, opacity: 60, shadows: false, geo: 'raster', orderable: true,
      memory: 24, info: { format: ['XYZ / WebP', 'XYZ / WebP'], crs: 'EPSG:3857', root: '{z}/{x}/{y}.webp', source: ['示例夜间影像', 'Sample night imagery'], attribution: ['© DigitalViewer 示例', '© DigitalViewer sample'] },
      stats: { visible: 62, resident: 84, requests: 0, failed: 0, cpu: 24, gpu: 30, textures: 62, queued: 0, download: 0, parse: 0, pack: 0, upload: 0 }
    },
    {
      id: 'grid', group: 'base', type: 'grid',
      name: ['经纬网格', 'Graticule'],
      sub: ['动态间距 · 10°', 'Adaptive · 10°'],
      kind: ['经纬网 · 椭球贴合', 'Graticule · ellipsoid aligned'],
      state: 'ready', visible: true, opacity: 100, shadows: false, geo: 'base',
      memory: 1, info: { format: ['动态经纬网', 'Adaptive graticule'], crs: 'WGS84', root: '—', source: ['内置', 'Built-in'], attribution: ['—', '—'] },
      stats: { visible: 0, resident: 0, requests: 0, failed: 0, cpu: 1, gpu: 1, textures: 0, queued: 0, download: 0, parse: 0, pack: 0, upload: 0 }
    },
    {
      id: 'terrain', group: 'terrain', type: 'dem',
      name: ['区域高程 DEM', 'Regional DEM'],
      sub: ['COG · 加载 82%', 'COG · Loading 82%'],
      kind: ['GeoTIFF / COG · 椭球高', 'GeoTIFF / COG · ellipsoidal height'],
      state: 'loading', progress: 82, visible: true, opacity: 100, shadows: true, geo: 'raster', orderable: true, exag: true,
      memory: 96, info: { format: ['COG / Float32', 'COG / Float32'], crs: 'EPSG:3857', root: 'region_dem_cog.tif', source: ['本地目录', 'Local folder'], attribution: ['—', '—'] },
      stats: { visible: 38, resident: 96, requests: 5, failed: 0, cpu: 96, gpu: 42, textures: 12, queued: 3, download: 2, parse: 3, pack: 1, upload: 1 }
    },
    {
      id: 'buildings', group: 'data3d', type: 'tiles',
      name: ['城市建筑 3D Tiles', 'City buildings 3D Tiles'],
      sub: ['3D Tiles 1.1 · 238 MB', '3D Tiles 1.1 · 238 MB'],
      kind: ['3D Tiles 1.1 · ECEF', '3D Tiles 1.1 · ECEF'],
      state: 'ready', visible: true, opacity: 100, shadows: true, geo: 'shared',
      memory: 238, info: { format: ['3D Tiles 1.1', '3D Tiles 1.1'], crs: 'WGS84 / ECEF', root: 'tileset.json', source: ['https://data.example.com/city/tileset.json', 'https://data.example.com/city/tileset.json'], attribution: ['© 示例城市数据', '© Sample city data'] },
      stats: { visible: 248, resident: 412, requests: 6, failed: 0, cpu: 238, gpu: 306, textures: 186, queued: 0, download: 2, parse: 1, pack: 1, upload: 2 },
      pick: {
        batch: '128', height: '42.6 m', lonlat: '121.4738°E, 31.2303°N',
        tile: '17/107878/53552', content: 'content/b3dm/128.b3dm',
        surface: 'triangles', triangle: '#3,184', normal: '(0.42, 0.86, 0.29)',
        distance: '2.41 km', local: '(-412.8, 96.3, 208.7)',
        ecef: '(-2,831,442.1, 4,675,238.9, 3,287,104.4)',
        batchTable: [
          [['名称', 'Name'], ['环球金融中心', 'Shanghai World Financial Center']],
          [['层数', 'Floors'], ['101', '101']],
          [['高度', 'Height'], ['492 m', '492 m']],
          [['类型', 'Type'], ['办公', 'Office']]
        ]
      }
    },
    {
      id: 'pointcloud', group: 'data3d', type: 'points',
      name: ['测绘点云', 'Survey point cloud'],
      sub: ['PNTS · 解析 72%', 'PNTS · Parsing 72%'],
      kind: ['3D Tiles PNTS · ECEF', '3D Tiles PNTS · ECEF'],
      state: 'loading', progress: 72, visible: true, opacity: 100, shadows: false, geo: 'shared',
      memory: 122, info: { format: ['PNTS', 'PNTS'], crs: 'WGS84 / ECEF', root: 'pointcloud/tileset.json', source: ['本地目录', 'Local folder'], attribution: ['© 示例测绘数据', '© Sample survey data'] },
      stats: { visible: 96, resident: 148, requests: 3, failed: 1, cpu: 122, gpu: 88, textures: 0, queued: 2, download: 0, parse: 3, pack: 0, upload: 1 },
      pick: {
        batch: '77', height: '128.4 m', lonlat: '121.4741°E, 31.2301°N',
        tile: '16/53939/26776', content: 'content/points/77.pnts',
        surface: 'points', triangle: '#482,193', normal: '—',
        distance: '1.92 km', local: '(82.4, 31.8, -146.2)',
        ecef: '(-2,831,361.7, 4,675,301.2, 3,287,052.8)',
        batchTable: [
          [['分类', 'Class'], ['建筑立面', 'Facade']],
          [['强度', 'Intensity'], ['46', '46']],
          [['回波', 'Return'], ['1 of 3', '1 of 3']],
          [['扫描', 'Scan'], ['2026-09', '2026-09']]
        ]
      }
    },
    {
      id: 'osgb', group: 'data3d', type: 'osg',
      name: ['园区 OSGB', 'Campus OSGB'],
      sub: ['PagedLOD · 已暂停', 'PagedLOD · Paused'],
      kind: ['OSGB · 局部场景', 'OSGB · Local scene'],
      state: 'paused', visible: true, opacity: 100, shadows: true, geo: 'local',
      memory: 62, info: { format: ['OSGB / zlib', 'OSGB / zlib'], crs: '局部场景', root: 'osgb/root.osgb', source: ['本地目录', 'Local folder'], attribution: ['—', '—'] },
      stats: { visible: 18, resident: 22, requests: 0, failed: 0, cpu: 62, gpu: 44, textures: 32, queued: 0, download: 0, parse: 0, pack: 0, upload: 0 },
      align: { lon: 121.4702, lat: 31.2286, h: 4.2, e: 0, n: 0, u: 0, yaw: 0, pitch: 0, roll: 0, up: 'Z-Up', scale: 'Uniform' }
    },
    {
      id: 'gltf', group: 'session', type: 'gltf',
      name: ['设备模型', 'Equipment model'],
      sub: ['glTF 2.0 · 就绪', 'glTF 2.0 · Ready'],
      kind: ['glTF 2.0 · 带骨骼动画', 'glTF 2.0 · skinned animation'],
      state: 'ready', visible: true, opacity: 100, shadows: true, geo: 'local', animation: true,
      memory: 14, info: { format: ['glTF 2.0 / GLB', 'glTF 2.0 / GLB'], crs: '局部场景', root: 'device.glb', source: ['本地文件', 'Local file'], attribution: ['—', '—'] },
      stats: { visible: 1, resident: 1, requests: 0, failed: 0, cpu: 14, gpu: 18, textures: 6, queued: 0, download: 0, parse: 0, pack: 0, upload: 0 },
      align: { lon: 121.4751, lat: 31.2299, h: 12.0, e: 4, n: -2, u: 0, yaw: 32, pitch: 0, roll: 0, up: 'Y-Up', scale: 'Uniform' }
    }
  ];

  const ERRORS = [
    {
      time: '12:04:12', layer: ['测绘点云', 'Survey point cloud'], stage: ['解析', 'Parse'],
      code: 'PNTS_NORMAL', level: 'warn',
      text: ['3 个点云批次缺少法线，已使用默认法线显示。', '3 point-cloud batches are missing normals; defaults were applied.'],
      suggestion: ['不影响显示，可在导出源数据时补齐法线。', 'Rendering is unaffected; add normals when exporting the source data.']
    },
    {
      time: '12:03:58', layer: ['园区 OSGB', 'Campus OSGB'], stage: ['下载', 'Download'],
      code: 'HTTP 403', level: 'error',
      text: ['服务器拒绝访问 tile_0.osgb。', 'The server refused access to tile_0.osgb.'],
      suggestion: ['检查 Token 是否过期，或该资源是否允许当前来源访问。', 'Check whether the token expired or the origin is allowed to access this resource.']
    }
  ];

  /* -------------------------------------------------------------- 静态文案表 */
  const ZH_EN = {
    '添加数据': 'Add data', '本地文件或目录': 'Local file or folder', '远程地址': 'Remote URL',
    'Cesium ion': 'Cesium ion', 'Asset ID 与会话 Token': 'Asset ID and session token',
    '飞往': 'Fly to', '视点': 'Viewpoints', '显示': 'Display', '经纬网': 'Graticule',
    'AI 助手': 'AI assistant', '脚本': 'Scripts', '轨道相机': 'Orbit camera', '飞行相机': 'Fly camera',
    '设置': 'Settings', '会话视点': 'Session viewpoints', '退出后清空': 'Cleared on exit',
    '保存当前视点': 'Save current viewpoint', '显示设置': 'Display settings', '全局': 'Global',
    '渲染模式': 'Render mode', '材质': 'Textured', '无光照': 'Unlit', '线框': 'Wireframe',
    '经纬网格': 'Graticule', '地形': 'Terrain', '投射阴影': 'Cast shadows',
    '图层': 'Layers', '本次会话 · 退出后清空': 'This session · cleared on exit',
    '属性': 'Properties', '配准': 'Registration', '拾取': 'Pick', '统计': 'Stats',
    '更多图层操作': 'More layer actions', '关闭': 'Close', '更多': 'More',
    '正在加载瓦片 68%': 'Loading tiles 68%', '高度': 'Height', '航向': 'Heading',
    '俯仰': 'Pitch', '视距': 'Distance', '复制': 'Copy', '复制坐标': 'Copy coordinates',
    '影像 © DigitalViewer 示例 · 数据 © 示例提供方': 'Imagery © DigitalViewer sample · Data © Sample provider',
    '重置视角': 'Reset view', '重置指南针': 'Reset compass', '放大': 'Zoom in', '缩小': 'Zoom out',
    '关闭拾取卡片': 'Close pick card', '取消飞行': 'Cancel flight',
    '展开图层面板': 'Show layers', '展开属性面板': 'Show properties',
    '折叠图层面板 (Ctrl+L)': 'Collapse layers (Ctrl+L)', '折叠属性面板 (Ctrl+P)': 'Collapse properties (Ctrl+P)',
    '返回属性': 'Back to properties', '关闭 AI 助手': 'Close AI assistant', '新聊天': 'New chat',
    '停止请求': 'Stop request', '发送': 'Send', '对话': 'Chat', '连接': 'Connection',
    '上下文': 'Context', '本地演示': 'Local demo',
    '描述你想调整的场景…': 'Describe the scene change you want…',
    'Enter 发送 · Shift+Enter 换行': 'Enter to send · Shift+Enter for newline',
    '仅当前会话保存连接信息，不写入项目或发布包。': 'Connection settings live only in this session; never written to the project or release.',
    '基础地址或完整请求地址': 'Base or full request URL', '模型名称': 'Model name',
    '访问凭据（可选）': 'Access credential (optional)', '超时（秒）': 'Timeout (seconds)',
    '输出预算': 'Output budget', '由服务决定': 'Service default', '连接并测试': 'Connect and test',
    '连接成功 · 模型响应 842 ms · 工具调用可用': 'Connected · model responded in 842 ms · tools available',
    '需要兼容 Chat Completions 的 messages、tools、tool_calls 与 role=tool。HTTP 会明文传输，请使用可信网络或 HTTPS。Web 端还需服务配置 CORS。': 'Requires Chat Completions with messages, tools, tool_calls, and role=tool. HTTP is sent in clear text; use a trusted network or HTTPS. Web also needs CORS.',
    '轮次': 'Rounds', '历史字节': 'History bytes', '摘要': 'Summaries', '执行事实': 'Facts',
    '本聊天摘要': 'Chat summary', '清空上下文': 'Clear context',
    '清空上下文不会移除当前场景和会话视点；切换连接或新聊天会同时清空撤销记录。': 'Clearing context keeps the current scene and session viewpoints. Switching connections or starting a new chat also clears undo history.',
    '原生 GDScript 工作台': 'Native GDScript workbench', '接管场景': 'Scene ownership',
    '权限说明': 'Permissions', '运行': 'Run', '停止': 'Stop', '重置场景': 'Reset scene',
    '打开': 'Open', '保存': 'Save', '示例': 'Examples', '已信任本次会话': 'Trusted this session',
    '输出': 'Output', '场景树': 'Scene tree',
    '原生脚本拥有应用权限；同步死循环无法通过“停止”中断。仅运行你信任的代码。AI 不会自动执行原生脚本。': 'Native scripts have app permissions; a synchronous infinite loop cannot be stopped by the Stop button. Run only code you trust. AI never runs native scripts automatically.',
    '运行中 · 场景所有权：native': 'Running · Scene owner: native',
    '运行中 · 00:42': 'Running · 00:42', 'GDScript · 主线程': 'GDScript · Main thread',
    '场景版本': 'Scene version', '只读': 'Read-only',
    '修改请通过脚本 API。': 'Edit through the script API.',
    '编译通过 · 0 个错误': 'Compiled · 0 errors',
    '已调用 run(viewer)，脚本实例已挂载': 'run(viewer) called; script instance mounted',
    '卫星已创建': 'Satellite created', '运行中': 'Running',
    '原生脚本拥有应用权限': 'Native scripts have app permissions',
    '脚本在主线程执行，可访问真实场景、文件、网络与平台能力。它不是安全沙箱。': 'Scripts run on the main thread and can access the real scene, files, network, and platform APIs. This is not a sandbox.',
    '同步死循环会阻塞主线程，停止按钮也无法响应': 'A synchronous infinite loop blocks the main thread; Stop cannot respond',
    '停止/重置只清理本次会话及场景，不保证文件、网络等副作用': 'Stop/reset clean this session and scene only; file or network side effects are not undone',
    '仅运行你信任的代码；AI 不会自动生成或执行原生脚本': 'Run only trusted code; AI never generates or runs native scripts automatically',
    '我已阅读并信任此脚本': 'I have read and trust this script', '运行原生 GDScript': 'Run native GDScript',
    '我信任并运行': 'Trust and run', '取消': 'Cancel',
    '状态与诊断': 'Status & diagnostics', '概览': 'Overview', '错误': 'Errors',
    '导出脱敏报告': 'Export redacted report', '清空': 'Clear', '阶段': 'Stage',
    '请求': 'Requests', '等待': 'Waiting', '可见': 'Visible', '失败': 'Failed', '内存': 'Memory',
    '就绪': 'Ready', '状态': 'Status', '网络': 'Network', '2 条警告': '2 warnings',
    '数据只存在于本次会话，关闭图层即释放': 'Data exists only in this session; removing a layer frees it',
    '本地文件': 'Local file', '本地目录': 'Local folder', 'ZIP 数据包': 'ZIP package',
    'WMTS 影像': 'WMTS imagery', '不保存路径、URL 或 Token': 'Paths, URLs, and tokens are not saved',
    '拖放文件到此处，或选择文件': 'Drop a file here, or choose one',
    '选择文件': 'Choose file', '显示名称': 'Display name', '识别为': 'Detect as',
    '自动识别': 'Auto-detect', '按需加载并保持相对路径': 'Load on demand and preserve relative paths',
    'ZIP 与目录会校验路径穿越、符号链接与压缩炸弹': 'ZIP and folders are checked for traversal, symlinks, and zip bombs',
    '选择一个数据目录': 'Choose a data folder',
    '用于 3D Tiles、OSGB 分页、预切片 DEM 或 glTF 外部依赖': 'For 3D Tiles, OSGB paging, tiled DEM, or glTF dependencies',
    '选择目录': 'Choose folder', '入口文件': 'Entry file', '自动查找 tileset.json': 'Find tileset.json automatically',
    '选择目录中的入口…': 'Choose an entry in the folder…',
    '目录数据在 Web 端会先复制到会话文件系统，保持相对路径。': 'On Web, folder data is copied into the session file system with relative paths preserved.',
    '选择 ZIP 数据包': 'Choose a ZIP package',
    '上限：单文件 4 GiB · 展开 16 GiB · 10,000 项': 'Limits: 4 GiB per file · 16 GiB expanded · 10,000 entries',
    '选择 ZIP': 'Choose ZIP',
    '拒绝路径穿越、绝对路径、符号链接与异常压缩比。解压到唯一会话目录，关闭图层即清理。': 'Rejects traversal, absolute paths, symlinks, and abnormal compression ratios. Extracts to a unique session folder cleaned up when the layer closes.',
    '存在多个入口时': 'Multiple entries', '显示入口选择器': 'Show entry picker',
    '自动选择唯一可识别项': 'Pick the only recognizable entry', '资源地址': 'Resource URL',
    '3D Tiles / tileset.json': '3D Tiles / tileset.json', 'XYZ 影像模板': 'XYZ imagery template', 'WMTS': 'WMTS',
    '远程 3D Tiles': 'Remote 3D Tiles',
    '超时 30 秒 · 重试 2 次 · 相同请求合并 · 不跳过 TLS 校验': '30 s timeout · 2 retries · request coalescing · TLS never skipped',
    '会话级自定义 Header': 'Session custom headers', 'Header 名': 'Header name', '值': 'Value',
    '仅本次会话': 'This session only', '敏感值会从日志与诊断报告中清除。': 'Sensitive values are removed from logs and diagnostics.',
    'GetCapabilities 地址': 'GetCapabilities URL', '读取': 'Read',
    '已识别 18 个矩阵层级，范围 0–17。v1 不支持 Dimension 与任意 EPSG。': '18 matrix levels detected, range 0–17. v1 does not support Dimensions or arbitrary EPSG.',
    'Asset ID': 'Asset ID', '资产类型': 'Asset type', 'Access Token': 'Access token',
    'Token 仅发送到 Cesium ion 官方 endpoint 与允许的同源 HTTPS 资产；不写入日志、配置、场景或 URL 历史。': 'The token is sent only to the official Cesium ion endpoint and allowed same-origin HTTPS assets; never logged or stored.',
    '添加': 'Add', '飞往坐标': 'Fly to coordinates', '输入经纬高，可选航向与俯仰': 'Enter lon/lat/height, optionally heading and pitch',
    '经度': 'Longitude', '纬度': 'Latitude', '观察高度 (km)': 'View height (km)',
    '航向角 (可选)': 'Heading (optional)', '俯仰角 (可选)': 'Pitch (optional)', '飞行时长': 'Flight duration',
    '自动（按距离）': 'Auto (by distance)', '当前': 'Current', '使用当前位置': 'Use current position',
    '飞行中按任意键或点击视口即可取消；也可使用顶部“取消飞行”。': 'Press any key or click the viewport to cancel; or use Cancel flight at the top.',
    '开始飞行': 'Start flight', 'v1 只持久化语言与非敏感显示偏好': 'v1 persists only language and non-sensitive display preferences',
    '语言': 'Language', '界面语言': 'Interface language', '默认跟随系统；未匹配语言回退英文': 'Follows the system by default; unsupported languages fall back to English',
    '简体中文': 'Simplified Chinese', 'English': 'English', '主题': 'Theme',
    'v1 正式发布仅深色；浅色为设计验证': 'v1 ships dark only; light is a design exploration',
    '深色': 'Dark', '浅色': 'Light', '坐标格式': 'Coordinate format',
    '影响 HUD、飞往与拾取面板': 'Affects the HUD, Fly to, and Pick panels',
    '十进制度': 'Decimal degrees', '度分秒 (DMS)': 'DMS', '减少动态效果': 'Reduce motion',
    '关闭面板过渡与相机缓动': 'Disable panel transitions and camera easing', '性能': 'Performance',
    'Resident 预算': 'Resident budget', '桌面 1 GiB · 移动/Web 384 MiB': 'Desktop 1 GiB · Mobile/Web 384 MiB',
    '每帧上传预算': 'Per-frame upload budget', '主线程资源上传上限': 'Main-thread upload cap',
    '隐私': 'Privacy',
    '不收集遥测，不集成第三方分析；不保存 Token、URL、本地路径、图层、配准或视点。诊断报告会自动移除敏感字段。': 'No telemetry or third-party analytics; tokens, URLs, local paths, layers, registration, and viewpoints are not stored. Diagnostics redact sensitive fields.',
    '完成': 'Done', '运行信任确认': 'Trust confirmation',
    'satellite.gd · 1.1 KB · 24 行': 'satellite.gd · 1.1 KB · 24 lines',
    '视图': 'View', '更多': 'More', '打开图层': 'Open layers', '飞往坐标': 'Fly to coordinates',
    '关闭图层': 'Close layers', '关闭面板': 'Close panel', '添加数据': 'Add data',
    '状态与诊断': 'Status & diagnostics', '文件选择使用系统 SAF / DocumentPicker；不保存路径与 Token。': 'Files use SAF / DocumentPicker; paths and tokens are not stored.',
    '模型、瓦片、ZIP': 'Models, tiles, ZIP', '分页数据与相对依赖': 'Paged data and dependencies',
    'AI 场景助手': 'AI scene assistant', '对话、连接与上下文': 'Chat, connection, and context',
    '脚本工作台': 'Script workbench', '原生 GDScript · 需信任': 'Native GDScript · trust required',
    '语言、显示、隐私': 'Language, display, privacy', '关于 DigitalViewer': 'About DigitalViewer',
    '高度 (km)': 'Height (km)', '航向 (可选)': 'Heading (optional)',
    '飞行中点击视口或按停止可取消。': 'Click the viewport or press stop to cancel.',
    '影像 © DigitalViewer 示例 · L17': 'Imagery © DigitalViewer sample · L17',
    '本次会话': 'This session', '权限': 'Permissions', '关闭对话框': 'Close dialog', '关于': 'About',
    '城市建筑 3D Tiles': 'City buildings 3D Tiles',
    '经纬高': 'Lon / lat / height',
    '飞往 121.4737°E, 31.2304°N': 'Fly to 121.4737°E, 31.2304°N',
    '已隐藏经纬网并飞往上海；用户认可当前视角。': 'Graticule hidden and flew to Shanghai; the viewpoint was accepted.',
    '保存了视点“总览”，未修改其他图层。': 'Saved viewpoint “Overview”; no other layer changed.',
    'viewpoint.save("总览")': 'viewpoint.save("Overview")',
    'UTF-8 · 24 行 · 1.1 KB': 'UTF-8 · 24 lines · 1.1 KB',
    '# 卫星示例：创建轨道立方体与光源': '# Satellite sample: orbit cube and light',
    '"卫星已创建"': '"Satellite created"',
    'Satellite · 只读': 'Satellite · Read-only',
    '属性检查只读；修改请通过脚本 API。': 'Inspector is read-only; edit through the script API.',
    '影像': 'Imagery', '1.5 秒': '1.5 s', '3 秒': '3 s', '6 秒': '6 s',
    '图层属性': 'Layer properties', '本地演示 · 会话内': 'Local demo · In-session',
    'Enter 发送': 'Enter to send', '删除': 'Delete', '关闭面板': 'Close panel',
    '网格间距': 'Grid spacing', '加载中': 'Loading', '已暂停': 'Paused', '部分失败': 'Partial failure',
    '点击视口开始拾取': 'Click the viewport to pick', '会话': 'Session',
    '添加后立即创建会话图层并进入排队；解析失败会进入底部诊断面板。': 'Adding creates a session layer and queues it immediately; parse failures appear in the diagnostics panel.',
    '应用菜单': 'Application menu', '版本、引擎与隐私说明': 'Version, engine, and privacy',
    '语言、显示与隐私': 'Language, display, and privacy', '退出应用': 'Quit application',
    '取消任务并清理会话': 'Cancel tasks and clean up the session',
    '跨平台三维与地理空间数据查看器': 'Cross-platform 3D and geospatial data viewer',
    '定制 Godot 4.7.3-rc · Compatibility 渲染器': 'Custom Godot 4.7.3-rc · Compatibility renderer',
    '包标识': 'Bundle ID', '渲染器': 'Renderer', '界面语言': 'Interface language',
    '会话数据': 'Session data', '简体中文 / English': 'Simplified Chinese / English',
    '不收集遥测，不集成第三方分析。Token、URL、本地路径、图层、配准与视点均不写入磁盘；诊断报告会自动脱敏。': 'No telemetry or third-party analytics. Tokens, URLs, local paths, layers, registration, and viewpoints are never written to disk; diagnostics are automatically redacted.',
    '退出 DigitalViewer？': 'Quit DigitalViewer?', '会话数据不会保存': 'Session data will not be saved',
    '取消下载、解析与上传任务': 'Cancel downloads, parsing, and uploads',
    '释放图层与 GPU 资源': 'Release layers and GPU resources',
    '清理会话临时文件': 'Clean up session temporary files',
    '退出后不会恢复图层、路径、URL、Token、配准或视点。': 'Layers, paths, URLs, tokens, registration, and viewpoints will not be restored after exit.',
    '退出': 'Quit', '关于': 'About'
  };

  function translateStatic() {
    const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, {
      acceptNode(node) {
        if (!node.nodeValue || !node.nodeValue.trim()) return NodeFilter.FILTER_REJECT;
        const p = node.parentElement;
        if (!p || p.closest('.sprite, script, style')) return NodeFilter.FILTER_REJECT;
        return NodeFilter.FILTER_ACCEPT;
      }
    });
    const nodes = [];
    while (walker.nextNode()) nodes.push(walker.currentNode);
    nodes.forEach((node) => {
      if (node.__zh === undefined) node.__zh = node.nodeValue;
      const raw = node.__zh;
      const key = raw.trim();
      const en = ZH_EN[key];
      if (!en) { node.nodeValue = state.lang === 'en' ? raw : raw; return; }
      const lead = raw.match(/^\s*/)[0];
      const trail = raw.match(/\s*$/)[0];
      node.nodeValue = state.lang === 'en' ? `${lead}${en}${trail}` : raw;
    });
    $$('[title], [placeholder], [aria-label]').forEach((el) => {
      ['title', 'placeholder', 'aria-label'].forEach((attr) => {
        if (!el.hasAttribute(attr)) return;
        const store = `zh${attr.replace('-', '')}`;
        if (el.dataset[store] === undefined) el.dataset[store] = el.getAttribute(attr);
        const key = el.dataset[store];
        const en = ZH_EN[key];
        if (en) el.setAttribute(attr, state.lang === 'en' ? en : key);
      });
    });
  }

  /* ------------------------------------------------------------- 渲染：图层 */
  const typeIcon = (type) => ({ globe: 'globe', imagery: 'image', grid: 'grid', dem: 'mountain', tiles: 'box', points: 'points', osg: 'archive', gltf: 'box' }[type] || 'box');

  function stateCell(layer) {
    if (layer.state === 'loading') return `<span class="state-ring" style="--p:${layer.progress || 0}" aria-label="${tx('loading')}"></span>`;
    if (layer.state === 'paused') return `<span class="state-paused"><svg class="ic"><use href="#i-pause"/></svg></span>`;
    const cls = layer.state === 'warn' ? 'warn' : layer.state === 'error' ? 'error' : layer.state === 'idle' ? 'idle' : '';
    return `<span class="state-dot ${cls}"></span>`;
  }

  function layerRow(layer, compact = false) {
    const selected = state.selected === layer.id ? 'selected' : '';
    const off = layer.visible ? '' : 'is-off';
    const eye = layer.visible ? 'eye' : 'eye-off';
    return `<div class="layer-row ${selected} ${off}" role="button" tabindex="0" data-layer="${layer.id}" aria-label="${esc(t(layer.name))}">
      <button class="eye-btn" type="button" data-eye="${layer.id}" aria-label="${layer.visible ? '隐藏' : '显示'} ${esc(t(layer.name))}" aria-pressed="${layer.visible}"><svg class="ic"><use href="#i-${eye}"/></svg></button>
      <span class="type-ic t-${layer.type}"><svg class="ic"><use href="#i-${typeIcon(layer.type)}"/></svg></span>
      <span class="layer-main">
        <span class="layer-name">${esc(t(layer.name))}</span>
        <span class="layer-sub">${esc(t(layer.sub))}</span>
      </span>
      <span class="layer-state">${stateCell(layer)}</span>
    </div>`;
  }

  function renderLayers() {
    const html = GROUPS.map((group) => {
      const rows = LAYERS.filter((l) => l.group === group.id);
      if (!rows.length) return '';
      const collapsed = state.collapsedGroups.has(group.id);
      return `<div class="layer-group ${collapsed ? 'collapsed' : ''}" data-group="${group.id}">
        <button class="group-head" type="button" data-group-toggle="${group.id}" aria-expanded="${!collapsed}">
          <svg class="ic chev"><use href="#i-chevron-down"/></svg>
          <span>${esc(t(group.name))}</span>
          <span class="g-count">${rows.length}</span>
        </button>
        <div class="group-rows">${rows.map((l) => layerRow(l)).join('')}</div>
      </div>`;
    }).join('');
    const desktop = $('#layerList');
    const mobile = $('#mLayerList');
    if (desktop) desktop.innerHTML = html;
    if (mobile) mobile.innerHTML = html;
    const count = LAYERS.length;
    if ($('#layerCount')) $('#layerCount').textContent = count;
    if ($('#mLayerCount')) $('#mLayerCount').textContent = count;
  }

  /* ------------------------------------------------------------- 渲染：属性 */
  const kv = (k, v, cls = '') => `<div class="kv"><span>${k}</span><span class="${cls}">${esc(v)}</span></div>`;
  const switchRow = (label, on, action) => `<div class="row-between"><span>${label}</span><button class="switch ${on ? 'is-on' : ''}" type="button" data-pa="${action}" role="switch" aria-checked="${on}" aria-label="${label}"></button></div>`;

  function sectionLabel(text) { return `<div class="section-label">${text}</div>`; }

  function propSource(layer) {
    let h = `<div class="prop-group">${sectionLabel(tx('source'))}`;
    h += kv(tx('format'), t(layer.info.format));
    h += kv(tx('crs'), layer.info.crs);
    h += kv(tx('root'), layer.info.root);
    h += kv(tx('sourceLabel'), t(layer.info.source));
    h += kv(tx('status'), t(layer.sub), layer.state === 'ready' ? 'accent' : layer.state === 'paused' ? 'warn' : layer.state === 'error' ? 'danger' : '');
    h += kv(tx('memory'), `${layer.memory.toFixed(1)} MB`);
    if (t(layer.info.attribution) !== '—') h += kv(tx('attribution'), t(layer.info.attribution));
    h += '</div><div class="prop-sep"></div>';
    h += `<div class="prop-group">${sectionLabel(tx('display'))}`;
    h += switchRow(tx('visible'), layer.visible, 'visibility');
    if (layer.type !== 'grid' && layer.type !== 'imagery') h += switchRow(tx('shadows'), layer.shadows !== false, 'shadows');
    h += `<div class="row-between" style="margin-top:6px"><span>${tx('opacity')}</span><span class="value-pill" id="opacityLabel">${layer.opacity}%</span></div>`;
    h += `<input type="range" min="0" max="100" value="${layer.opacity}" data-pa="opacity" aria-label="${tx('opacity')}">`;
    if (layer.orderable) {
      h += `<div class="row-between"><span>${tx('order')}</span><span class="order-row">
        <button class="icon-btn tiny" type="button" data-pa="order-up" title="${tx('moveUp')}" aria-label="${tx('moveUp')}"><svg class="ic"><use href="#i-arrow-up"/></svg></button>
        <button class="icon-btn tiny" type="button" data-pa="order-down" title="${tx('moveDown')}" aria-label="${tx('moveDown')}"><svg class="ic"><use href="#i-arrow-down"/></svg></button>
      </span></div><p class="hint">${tx('orderHint')}</p>`;
    }
    if (layer.animation) {
      h += `<div class="prop-sep"></div>${sectionLabel(tx('animation'))}
        <div class="row-between"><span>Take 01 · 06:40</span><button class="btn sm" type="button" data-pa="anim"><svg class="ic"><use href="#i-pause"/></svg>${tx('pause')}</button></div>
        <input type="range" min="0" max="100" value="32" aria-label="animation timeline">
        <div class="row-between"><span class="hint">02:14 / 06:40</span><select class="compact" aria-label="${tx('speed')}"><option>0.5×</option><option selected>1×</option><option>2×</option></select></div>`;
    }
    h += '</div>';
    if (layer.exag) {
      h += `<div class="prop-sep"></div><div class="prop-group">${sectionLabel(tx('terrain'))}
        <div class="row-between"><span>${tx('exag')}</span><span class="value-pill" id="exagLabel">${state.exag.toFixed(1)}×</span></div>
        <input type="range" min="0" max="10" step="0.1" value="${state.exag}" data-pa="exag">
        <div class="prop-sep"></div>
        <label class="field"><span>${tx('exagNum')}</span><input type="number" min="0" max="100" step="0.1" value="${state.exag}" data-pa="exag-num"></label>
        <p class="hint">${tx('exagHint')}</p></div>`;
    }
    if (layer.pick) {
      h += `<div class="notice"><svg class="ic"><use href="#i-crosshair"/></svg><span>${tx('pickHint')}</span></div>`;
    }
    h += `<div class="prop-actions"><button class="btn sm" type="button" data-pa="focus"><svg class="ic"><use href="#i-focus"/></svg>${tx('focus')}</button></div>`;
    return h;
  }

  function field(label, value, opts = '') {
    return `<label class="field"><span>${label}</span><input value="${esc(value)}" ${opts}></label>`;
  }

  function propAlign(layer) {
    if (layer.geo === 'base') {
      return `<div class="empty-state"><div class="empty-ic"><svg class="ic"><use href="#i-globe"/></svg></div><b>${tx('alignInfo')}</b><p>${tx('baseAlignBody')}</p></div>`;
    }
    if (layer.geo === 'shared' || layer.geo === 'raster') {
      let h = `<div class="notice ok"><svg class="ic"><use href="#i-check"/></svg><span>${tx('alignInfoBody')}</span></div>`;
      h += sectionLabel(tx('register'));
      h += kv(tx('rasterCrs'), layer.info.crs);
      if (layer.geo === 'raster') {
        h += kv(tx('rasterScheme'), layer.type === 'imagery' ? 'XYZ / TMS' : 'GeoTIFF north-up');
        h += kv(tx('bounds'), layer.type === 'imagery' ? '85.05°S – 85.05°N · 180°W – 180°E' : '30.98°N – 31.52°N · 121.20°E – 121.78°E');
      }
      h += `<div class="prop-sep"></div><div class="prop-group">${switchRow(tx('forceEcef'), false, 'force-ecef')}<p class="hint">${tx('forceEcefHint')}</p></div>`;
      return h;
    }
    const a = layer.align || {};
    let h = `<div class="prop-group">${sectionLabel(tx('localMode'))}
      <select class="compact wide" aria-label="${tx('localMode')}"><option selected>${tx('anchor')}</option><option>${tx('localScene')}</option></select>
    </div>
    <div class="prop-group">${sectionLabel(tx('anchor'))}
      <div class="form-row three">
        ${field(tx('lon'), a.lon, 'inputmode="decimal"')}
        ${field(tx('lat'), a.lat, 'inputmode="decimal"')}
        ${field(tx('height'), a.h, 'inputmode="decimal"')}
      </div>
    </div>
    <div class="prop-group">${sectionLabel(tx('enu'))}
      <div class="form-row three">
        ${field(tx('east'), a.e, 'inputmode="decimal"')}
        ${field(tx('north'), a.n, 'inputmode="decimal"')}
        ${field(tx('up'), a.u, 'inputmode="decimal"')}
      </div>
    </div>
    <div class="prop-group">${sectionLabel(tx('attitude'))}
      <div class="form-row three">
        ${field(tx('yaw'), a.yaw, 'inputmode="decimal"')}
        ${field(tx('pitch'), a.pitch, 'inputmode="decimal"')}
        ${field(tx('roll'), a.roll, 'inputmode="decimal"')}
      </div>
    </div>
    <div class="prop-group">${sectionLabel(tx('origin'))}
      <div class="form-row three">${field('X', '0')}${field('Y', '0')}${field('Z', '0')}</div>
      <div class="row-between"><span>${tx('upAxis')}</span><select class="compact" aria-label="${tx('upAxis')}"><option ${a.up === 'Z-Up' ? 'selected' : ''}>Z-Up</option><option ${a.up === 'Y-Up' ? 'selected' : ''}>Y-Up</option></select></div>
      <div class="row-between"><span>${tx('scale')}</span><select class="compact" aria-label="${tx('scale')}"><option>${tx('uniformScale')}</option><option>${tx('perAxis')}</option></select></div>
      <div class="row-between"><span>${tx('gizmo')}</span><button class="switch" type="button" data-pa="gizmo" role="switch" aria-checked="false" aria-label="${tx('gizmo')}"></button></div>
    </div>
    <div class="prop-actions"><button class="btn sm" type="button" data-pa="align-reset">${tx('reset')}</button><button class="btn sm primary" type="button" data-pa="align-apply">${tx('apply')}</button></div>`;
    return h;
  }

  function propPick(layer) {
    if (!layer.pick) {
      return `<div class="empty-state"><div class="empty-ic"><svg class="ic"><use href="#i-crosshair"/></svg></div><b>${tx('pickEmpty')}</b><p>${tx('pickEmptyBody')}</p></div>`;
    }
    const p = layer.pick;
    let h = `<div class="pick-result">
      <div class="pick-result-head"><span class="type-ic t-${layer.type}"><svg class="ic"><use href="#i-${typeIcon(layer.type)}"/></svg></span>${esc(t(layer.name))}<span class="grow"></span><span class="chip ok">${tx('pickTab')}</span></div>
      <div class="pick-result-body">`;
    h += kv(tx('batch'), p.batch);
    h += kv(tx('lonlat'), p.lonlat);
    h += kv(tx('height'), p.height);
    h += kv(tx('tile'), p.tile);
    h += kv(tx('content'), p.content);
    h += kv(tx('surface'), p.surface);
    h += kv(tx('triangle'), p.triangle);
    h += kv(tx('normal'), p.normal);
    h += kv(tx('distance'), p.distance);
    h += kv(tx('localPos'), p.local);
    h += kv(tx('ecef'), p.ecef);
    h += `</div></div>`;
    h += `<div class="prop-group">${sectionLabel(tx('batchTable'))}<div class="batch-table">`;
    p.batchTable.forEach((row) => { h += `<div class="row"><span>${esc(t(row[0]))}</span><b>${esc(t(row[1]))}</b></div>`; });
    h += `</div></div>`;
    h += `<div class="prop-actions"><button class="btn sm" type="button" data-pa="copy-pick"><svg class="ic"><use href="#i-copy"/></svg>${tx('copy')}</button><button class="btn sm primary" type="button" data-pa="fly-pick"><svg class="ic"><use href="#i-focus"/></svg>${tx('pickFly')}</button></div>`;
    return h;
  }

  function propStats(layer) {
    const s = layer.stats;
    let h = `<div class="prop-group">${sectionLabel(tx('stats'))}<div class="stats-grid">
      <div class="stat-box"><span>${tx('visibleTiles')}</span><b>${s.visible}</b></div>
      <div class="stat-box"><span>${tx('requests')}</span><b>${s.requests}</b></div>
      <div class="stat-box"><span>${tx('resident')}</span><b>${s.resident}</b></div>
      <div class="stat-box"><span>${tx('failed')}</span><b>${s.failed}</b></div>
    </div></div>`;
    h += `<div class="prop-group">${sectionLabel(tx('memory'))}`;
    h += kv(tx('cpu'), `${s.cpu.toFixed(1)} MB`);
    h += kv(tx('gpu'), `${s.gpu.toFixed(1)} MB`);
    h += kv(tx('textures'), `${s.textures}`);
    h += kv(tx('memory'), `${layer.memory.toFixed(1)} MB`);
    h += `</div>`;
    if (['tiles', 'points', 'osg'].includes(layer.type)) {
      h += `<div class="prop-group">${sectionLabel(tx('pipeline'))}<div class="pipeline">`;
      const max = Math.max(1, s.queued, s.download, s.parse, s.pack, s.upload);
      [['queued', tx('queued')], ['download', tx('download')], ['parse', tx('parse')], ['pack', tx('pack')], ['upload', tx('upload')]].forEach(([key, label]) => {
        const val = s[key] || 0;
        h += `<div class="pipe-row"><span>${label}</span><span class="pipe-bar"><i style="width:${Math.round((val / max) * 100)}%"></i></span><b>${val}</b></div>`;
      });
      h += `</div></div>`;
    }
    return h;
  }

  function renderProps() {
    const layer = LAYERS.find((l) => l.id === state.selected) || LAYERS[0];
    $('#propTitle').textContent = t(layer.name);
    $('#propSub').textContent = t(layer.sub);
    $$('#propTabs [data-tab]').forEach((btn) => btn.classList.toggle('is-on', btn.dataset.tab === state.tab));
    const content = $('#propContent');
    let html = '';
    if (state.tab === 'source') html = propSource(layer);
    else if (state.tab === 'align') html = propAlign(layer);
    else if (state.tab === 'pick') html = propPick(layer);
    else html = propStats(layer);
    content.innerHTML = html;
    bindPropUI(content);
    renderMobileProps(layer);
  }

  function renderMobileProps(layer) {
    const content = $('#mPropContent');
    if (!content) return;
    let h = `<div class="m-prop-head"><span class="type-ic t-${layer.type}"><svg class="ic"><use href="#i-${typeIcon(layer.type)}"/></svg></span><div><b>${esc(t(layer.name))}</b><span>${esc(t(layer.kind))}</span></div></div>`;
    h += `<div class="prop-group">${sectionLabel(tx('source'))}`;
    h += kv(tx('format'), t(layer.info.format));
    h += kv(tx('crs'), layer.info.crs);
    h += kv(tx('status'), t(layer.sub), layer.state === 'ready' ? 'accent' : 'warn');
    h += `</div><div class="prop-sep"></div><div class="prop-group">${sectionLabel(tx('display'))}`;
    h += switchRow(tx('visible'), layer.visible, 'visibility');
    h += `<div class="row-between"><span>${tx('opacity')}</span><span class="value-pill" id="opacityLabel">${layer.opacity}%</span></div>
      <input type="range" min="0" max="100" value="${layer.opacity}" data-pa="opacity" aria-label="${tx('opacity')}">`;
    if (layer.exag) {
      h += `<div class="row-between" style="margin-top:8px"><span>${tx('exag')}</span><span class="value-pill" id="exagLabel">${state.exag.toFixed(1)}×</span></div>
        <input type="range" min="0" max="10" step="0.1" value="${state.exag}" data-pa="exag">`;
    }
    h += `</div>`;
    if (layer.pick) h += `<div class="prop-group">${sectionLabel(tx('pickTab'))}${kv(tx('batch'), layer.pick.batch)}${kv(tx('lonlat'), layer.pick.lonlat)}<p class="hint">${tx('pickEmptyBody')}</p></div>`;
    h += `<div class="prop-actions"><button class="btn sm" type="button" data-pa="focus"><svg class="ic"><use href="#i-focus"/></svg>${tx('focus')}</button></div>`;
    content.innerHTML = h;
    bindPropUI(content);
  }

  /* --------------------------------------------------------- 属性交互绑定 */
  function bindPropUI(root) {
    $$('[data-pa]', root).forEach((el) => {
      const action = el.dataset.pa;
      if (action === 'opacity') {
        el.addEventListener('input', () => {
          const layer = LAYERS.find((l) => l.id === state.selected);
          if (!layer) return;
          layer.opacity = Number(el.value);
          const label = root.querySelector('#opacityLabel');
          if (label) label.textContent = `${layer.opacity}%`;
          const rowSub = layerRowSub(layer);
          if (rowSub) rowSub.textContent = t(layer.sub);
        });
      } else if (action === 'exag' || action === 'exag-num') {
        el.addEventListener('input', () => {
          const value = clamp(Number(el.value), 0, action === 'exag' ? 10 : 100);
          state.exag = value;
          const label = root.querySelector('#exagLabel');
          if (label) label.textContent = `${value.toFixed(1)}×`;
          root.querySelectorAll('[data-pa="exag"], [data-pa="exag-num"]').forEach((input) => {
            if (input !== el) input.value = value;
          });
        });
      } else {
        el.addEventListener('click', () => handlePropAction(action));
      }
    });
  }

  function layerRowSub(layer) {
    const rows = $$(`[data-layer="${layer.id}"] .layer-sub`);
    return rows[0];
  }

  function handlePropAction(action) {
    const layer = LAYERS.find((l) => l.id === state.selected);
    if (!layer) return;
    if (action === 'visibility') {
      toggleLayer(layer.id);
    } else if (action === 'shadows') {
      layer.shadows = !layer.shadows;
      renderProps();
      toast(`${tx('shadows')}: ${layer.shadows ? 'ON' : 'OFF'}`);
    } else if (action === 'order-up' || action === 'order-down') {
      moveLayer(layer, action === 'order-up' ? -1 : 1);
    } else if (action === 'focus') {
      setStatus(`${tx('focusToast')} “${t(layer.name)}”`);
      toast(`${tx('focusToast')} · ${t(layer.name)}`);
    } else if (action === 'anim') {
      toast(`${tx('animation')} · ${tx('play')}`);
    } else if (action === 'align-apply') {
      toast(`${tx('apply')} · ${t(layer.name)}`);
      setStatus(`${tx('apply')} · ${t(layer.name)}`);
    } else if (action === 'align-reset') {
      toast(`${tx('reset')} · ${t(layer.name)}`);
    } else if (action === 'gizmo') {
      toast(`${tx('gizmo')}: ON`);
    } else if (action === 'force-ecef') {
      toast('ECEF · ON');
    } else if (action === 'copy-pick') {
      toast(tx('copied'));
    } else if (action === 'fly-pick' && layer.pick) {
      openFly(layer.pick);
    }
  }

  function moveLayer(layer, dir) {
    const group = LAYERS.filter((l) => l.group === layer.group);
    const idx = group.indexOf(layer);
    const target = idx + dir;
    if (target < 0 || target >= group.length) {
      toast(tx('orderHint'));
      return;
    }
    const a = LAYERS.indexOf(group[idx]);
    const b = LAYERS.indexOf(group[target]);
    [LAYERS[a], LAYERS[b]] = [LAYERS[b], LAYERS[a]];
    renderLayers();
    renderProps();
    toast(`${tx('order')} · ${t(layer.name)}`);
  }

  function toggleLayer(id) {
    const layer = LAYERS.find((l) => l.id === id);
    if (!layer) return;
    layer.visible = !layer.visible;
    if (layer.visible) {
      layer.state = layer.id === 'terrain' || layer.id === 'pointcloud' ? 'loading' : 'ready';
    }
    renderLayers();
    if (state.selected === id) renderProps();
    if (id === 'grid') state.grid = layer.visible;
    setStatus(`${layer.visible ? tx('visible') : '隐藏'} “${t(layer.name)}”`);
    toast(`${layer.visible ? tx('visible') : '隐藏'} · ${t(layer.name)}`);
  }

  /* --------------------------------------------------------------- 渲染：诊断 */
  function stageChip(layer) {
    const map = {
      ready: ['ready', tx('ready')], loading: ['loading', tx('loading')],
      paused: ['paused', tx('paused')], warn: ['warn', tx('warnState')], error: ['error', tx('failedState')], idle: ['', tx('idle')]
    };
    const [cls, label] = map[layer.state] || map.idle;
    return `<span class="stage-chip ${cls}">${label}</span>`;
  }

  function renderDiag() {
    const body = $('#diagBody');
    if (body) {
      body.innerHTML = LAYERS.map((l) => `<tr>
        <td>${esc(t(l.name))}</td>
        <td>${stageChip(l)}</td>
        <td class="num">${l.stats.requests}</td>
        <td class="num">${l.stats.queued}</td>
        <td class="num">${l.stats.resident}</td>
        <td class="num">${l.stats.visible}</td>
        <td class="num">${l.stats.failed}</td>
        <td class="num">${l.memory.toFixed(1)} MB</td>
      </tr>`).join('');
    }
    const errorsHtml = ERRORS.map((e, index) => `<div class="error-item">
      <span class="err-time">${e.time}</span>
      <div class="err-main">
        <div class="err-head"><b>${esc(t(e.layer))}</b><span class="stage-chip">${esc(t(e.stage))}</span><span class="err-code ${e.level === 'warn' ? 'warn' : ''}">${e.code}</span></div>
        <p>${esc(t(e.text))} ${esc(t(e.suggestion))}</p>
      </div>
      <div class="err-actions"><button class="icon-btn tiny" type="button" data-error-copy="${index}" aria-label="${tx('copy')}"><svg class="ic"><use href="#i-copy"/></svg></button><button class="icon-btn tiny" type="button" data-error-retry="${index}" aria-label="${tx('reload')}"><svg class="ic"><use href="#i-refresh"/></svg></button></div>
    </div>`).join('');
    if ($('#errorList')) $('#errorList').innerHTML = errorsHtml;
    if ($('#mErrorList')) $('#mErrorList').innerHTML = errorsHtml;
    const total = LAYERS.reduce((sum, l) => sum + l.memory, 0);
    if ($('#residentText')) $('#residentText').textContent = `${Math.round(total)} MB / 1 GB`;
    if ($('#residentBar')) $('#residentBar').style.width = `${Math.min(100, (total / 1024) * 100)}%`;
  }

  /* ----------------------------------------------------------- 渲染：视点 */
  function renderViewpoints() {
    const list = $('#viewpointList');
    if (!list) return;
    if (!state.points.length) {
      list.innerHTML = `<div class="vp-empty">${state.lang === 'en' ? 'No viewpoints in this session.' : '本次会话还没有视点。'}</div>`;
    } else {
      list.innerHTML = state.points.map((p, index) => `<div class="vp-row">
        <button class="vp-go" type="button" data-vp-go="${index}">${esc(t(p.name))}<small>${p.lon.toFixed(4)}°E · ${p.lat.toFixed(4)}°N · ${p.h.toFixed(2)} km</small></button>
        <button class="icon-btn tiny" type="button" data-vp-go="${index}" aria-label="${state.lang === 'en' ? 'Fly to' : '飞往'}"><svg class="ic"><use href="#i-navigation"/></svg></button>
        <button class="icon-btn tiny" type="button" data-vp-del="${index}" aria-label="${state.lang === 'en' ? 'Delete' : '删除'}"><svg class="ic"><use href="#i-trash"/></svg></button>
      </div>`).join('');
    }
    if ($('#viewpointBadge')) $('#viewpointBadge').textContent = state.points.length;
  }

  /* --------------------------------------------------------------- 渲染：AI */
  function messageHtml(m) {
    if (m.role === 'user') {
      return `<div class="msg user"><div class="bubble">${esc(tt(m.text))}</div></div>`;
    }
    const tools = (m.tools || []).map((tool) => `<div class="tool-call">
      <div class="tool-line"><span class="ok">✓</span><code>${tool.name}</code><span>${esc(t(tool.summary))}</span></div>
      <div class="tool-args">${esc(tool.args)}</div>
    </div>`).join('');
    const note = m.note ? `<div class="msg-note"><svg class="ic"><use href="#i-check"/></svg>${esc(t(m.note))}</div>` : '';
    return `<div class="msg"><div class="who"><span class="avatar"><svg class="ic"><use href="#i-sparkles"/></svg></span>${tx('aiName')}</div>${tools}<div class="bubble">${esc(tt(m.text))}</div>${note}</div>`;
  }

  function renderChat() {
    const html = state.chat.map(messageHtml).join('');
    ['#chatLog', '#mChatLog'].forEach((sel) => {
      const log = $(sel);
      if (!log) return;
      log.innerHTML = html;
      log.scrollTop = log.scrollHeight;
    });
    const suggests = state.device === 'mobile'
      ? (state.lang === 'en' ? T.aiSuggestMobileEn : T.aiSuggestMobile)
      : (state.lang === 'en' ? T.aiSuggestEn : T.aiSuggest);
    ['#chatSuggest', '#mChatSuggest'].forEach((sel) => {
      const box = $(sel);
      if (!box) return;
      box.innerHTML = suggests.map((label) => `<button type="button" data-suggest="${esc(label)}">${esc(label)}</button>`).join('');
    });
  }

  function sendChat(rawText) {
    const input = $('#chatInput');
    const text = (rawText || (input ? input.value : '') || '').trim();
    if (!text) return;
    state.chat.push({ role: 'user', text });
    if (input) input.value = '';
    renderChat();
    if (state.device === 'mobile') {
      setTimeout(() => {
        state.chat.push({ role: 'assistant', text: T.aiMobileReadonly });
        renderChat();
      }, 420);
      return;
    }
    const lower = text.toLowerCase();
    setTimeout(() => {
      let reply = { role: 'assistant', text: T.aiReply, tools: [{ name: 'script_propose', summary: ['GDScript 草案', 'GDScript draft'], args: 'extends Node\n\nfunc _ready() -> void:\n    pass' }], note: T.aiAction };
      if (/隐藏|hide|grid|经纬/.test(lower)) {
        reply = { role: 'assistant', text: T.aiHideGrid, tools: [{ name: 'script_propose', summary: ['隐藏经纬网', 'hide graticule'], args: 'extends Node\n\nfunc _ready() -> void:\n    for vp in viewer.viewports():\n        vp.set_grid(false)' }], note: T.aiAction };
      } else if (/飞往|fly|上海|shanghai/.test(lower)) {
        reply = { role: 'assistant', text: T.aiFly, tools: [{ name: 'script_propose', summary: ['飞往上海', 'fly to Shanghai'], args: 'extends Node\n\nfunc _ready() -> void:\n    viewer.active_viewport().fly_to(121.4737, 31.2304, 8.42)' }], note: T.aiAction };
      } else if (/视点|viewpoint|保存/.test(lower)) {
        reply = { role: 'assistant', text: T.aiViewpoint, tools: [{ name: 'script_propose', summary: ['保存当前视点', 'save current viewpoint'], args: 'extends Node\n\nfunc _ready() -> void:\n    viewer.save_viewpoint("视点 3")' }], note: T.aiAction };
      } else if (/圆环|ring|球/.test(lower)) {
        reply = { role: 'assistant', text: T.aiRing, tools: [{ name: 'script_propose', summary: ['创建 12 个球体', 'create 12 spheres'], args: 'extends Node\n\nfunc _ready() -> void:\n    for i in 12:\n        viewer.create_sphere(i)' }], note: T.aiAction };
      }
      state.chat.push(reply);
      renderChat();
    }, 620);
  }

  /* ------------------------------------------------------------- 状态与提示 */
  let statusTimer = null;
  function setStatus(text) {
    const el = $('#statusText');
    if (!el) return;
    el.textContent = text;
    clearTimeout(statusTimer);
    statusTimer = setTimeout(() => { el.textContent = tx('statusDefault'); }, 3200);
  }

  let toastTimer = null;
  function toast(text) {
    const el = $('#toast');
    if (!el) return;
    el.textContent = text;
    el.classList.add('show');
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => el.classList.remove('show'), 2400);
  }

  let mToastTimer = null;
  function mToast(text) {
    const el = $('#mToast');
    if (!el) return;
    el.textContent = text;
    el.classList.add('show');
    clearTimeout(mToastTimer);
    mToastTimer = setTimeout(() => el.classList.remove('show'), 2200);
  }

  /* ------------------------------------------------------------------ HUD */
  function updateHud() {
    const c = state.coords;
    const lonDir = c.lon >= 0 ? 'E' : 'W';
    const latDir = c.lat >= 0 ? 'N' : 'S';
    const lon = Math.abs(c.lon).toFixed(4);
    const lat = Math.abs(c.lat).toFixed(4);
    if ($('#hudLon')) $('#hudLon').textContent = `${lon}° ${lonDir}`;
    if ($('#hudLat')) $('#hudLat').textContent = `${lat}° ${latDir}`;
    if ($('#hudHeight')) $('#hudHeight').textContent = `${c.h.toFixed(2)} km`;
    if ($('#hudHeading')) $('#hudHeading').textContent = `${Math.round(c.heading)}°`;
    if ($('#hudPitch')) $('#hudPitch').textContent = `${Math.round(c.pitch)}°`;
    if ($('#hudDistance')) $('#hudDistance').textContent = `${c.distance.toFixed(1)} km`;
    if ($('#mHudLon')) $('#mHudLon').textContent = `${lon}° ${lonDir}`;
    if ($('#mHudLat')) $('#mHudLat').textContent = `${lat}° ${latDir}`;
    if ($('#mHudHeight')) $('#mHudHeight').textContent = `${c.h.toFixed(2)} km`;
    if ($('#flyCurrent')) $('#flyCurrent').textContent = `${lon}°${lonDir}, ${lat}°${latDir} · ${c.h.toFixed(2)} km`;
  }

  let flightTimer = null;
  function showFlight(text) {
    const chip = $('#flightChip');
    if (!chip) return;
    if ($('#flightText')) $('#flightText').textContent = text;
    chip.hidden = false;
    clearTimeout(flightTimer);
    flightTimer = setTimeout(() => { chip.hidden = true; }, 3000);
  }

  /* ------------------------------------------------------------ 弹窗/弹层 */
  function closePopovers() {
    $$('.pop-wrap.open').forEach((w) => w.classList.remove('open'));
    $$('[data-pop]').forEach((btn) => btn.setAttribute('aria-expanded', 'false'));
  }

  function closeModals() { $$('.modal.is-open').forEach((m) => m.classList.remove('is-open')); }

  function openModal(name) {
    closePopovers();
    closeModals();
    const modal = $(`.modal[data-modal="${name}"]`);
    if (modal) modal.classList.add('is-open');
    if (name === 'fly') {
      const err = $('#flyError');
      if (err) err.hidden = true;
    }
  }

  function setRight(mode) {
    state.right = mode;
    const app = $('#desktopApp');
    app.dataset.right = mode;
    if (mode !== 'none') {
      state.rightCollapsed = false;
      app.classList.remove('right-collapsed');
    }
    const aiBtn = $('#aiButton');
    if (aiBtn) aiBtn.classList.toggle('is-on', mode === 'ai');
  }

  function setDiag(open) {
    state.diag = open;
    $('#desktopApp').dataset.diag = open ? '1' : '0';
    const toggle = $('#diagToggle');
    if (toggle) toggle.setAttribute('aria-expanded', String(open));
    if (!open) return;
    // 首次展开时强制一次完整重绘，避免高度动画后表格行未失效（Chromium 合成层边界）。
    requestAnimationFrame(() => {
      const table = $('.diag-table');
      if (!table) return;
      table.style.transform = 'translateZ(0)';
      void table.offsetHeight;
      requestAnimationFrame(() => { table.style.transform = ''; });
    });
  }

  function setFullscreen(open) {
    state.fullscreen = open;
    const app = $('#desktopApp');
    app.dataset.full = open ? '1' : '0';
    const btn = $('#fullButton');
    if (btn) {
      btn.innerHTML = `<svg class="ic"><use href="#i-${open ? 'minimize' : 'maximize'}"/></svg>`;
      btn.title = open ? '退出全屏视口 (Esc)' : '全屏视口 (Ctrl+Shift+F)';
    }
  }

  function switchScriptTabs(tab) {
    state.scriptTab = tab;
    $$('#scTabs [data-sc-tab]').forEach((b) => b.classList.toggle('is-on', b.dataset.scTab === tab));
    $$('[data-sc-pane]').forEach((p) => { p.hidden = p.dataset.scPane !== tab; });
  }

  function openScript() {
    closeModals();
    setRight('props');
    const app = $('#desktopApp');
    app.dataset.script = '1';
    $('#scriptButton')?.classList.add('is-on');
    if ($('#runScript')) $('#runScript').disabled = state.running;
    if ($('#stopScript')) $('#stopScript').disabled = !state.running;
  }

  function closeScript() {
    $('#desktopApp').dataset.script = '0';
    $('#scriptButton')?.classList.remove('is-on');
  }

  function openFly(pick) {
    openModal('fly');
    if (!pick) return;
    const match = /([\d.]+)°E,\s*([\d.]+)°N/.exec(pick.lonlat || '');
    if (match) {
      if ($('#flyLon')) $('#flyLon').value = match[1];
      if ($('#flyLat')) $('#flyLat').value = match[2];
    }
    const heightM = parseFloat(pick.height);
    if (Number.isFinite(heightM) && $('#flyHeight')) {
      $('#flyHeight').value = Math.max(0.05, heightM / 1000 + 2).toFixed(2);
    }
    const err = $('#flyError');
    if (err) err.hidden = true;
  }

  function logScript(text, cls = '') {
    const log = $('#scriptLog');
    if (!log) return;
    const now = new Date();
    const line = document.createElement('div');
    line.innerHTML = `<time>${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}</time><span class="${cls || 'ok'}">${cls === 'info' ? 'i' : cls === 'err' ? '!' : '✓'}</span> ${esc(text)}`;
    log.appendChild(line);
    log.scrollTop = log.scrollHeight;
  }

  function runScript() {
    if (!state.trusted) { openModal('trust'); return; }
    state.running = true;
    if ($('#runScript')) $('#runScript').disabled = true;
    if ($('#stopScript')) $('#stopScript').disabled = false;
    state.sceneVersion += 1;
    if ($('#sceneVersion')) $('#sceneVersion').textContent = state.sceneVersion;
    logScript(tx('scriptRun'));
    setStatus(tx('scriptRun'));
    toast(tx('scriptRun'));
  }

  function stopScript() {
    state.running = false;
    if ($('#runScript')) $('#runScript').disabled = false;
    if ($('#stopScript')) $('#stopScript').disabled = true;
    logScript(tx('scriptStop'), 'err');
    toast(tx('scriptStop'));
  }

  /* --------------------------------------------------------------- 添加数据 */
  function selectSource(source) {
    $$('#sourceRail [data-source]').forEach((b) => b.classList.toggle('is-on', b.dataset.source === source));
    $$('[data-add-pane]').forEach((p) => { p.hidden = p.dataset.addPane !== source; });
  }

  function addLayerFromSource(source) {
    const names = {
      local: ['新建本地数据', 'New local data'], dir: ['新建目录数据', 'New folder data'],
      zip: ['新建 ZIP 数据', 'New ZIP data'], remote: ['新建远程数据', 'New remote data'],
      wmts: ['WMTS 影像', 'WMTS imagery'], ion: ['Cesium ion 资产', 'Cesium ion asset']
    };
    let pair = names[source] || names.local;
    if (source === 'remote') {
      const url = $('#remoteUrl') ? $('#remoteUrl').value.trim() : '';
      const last = url.split('/').filter(Boolean).pop();
      if (last) pair = [last, last];
    }
    if (source === 'local' || source === 'dir' || source === 'zip') {
      const input = $('#addName');
      if (input && input.value.trim()) pair = [input.value.trim(), input.value.trim()];
    }
    const type = source === 'wmts' ? 'imagery' : source === 'ion' ? 'tiles' : 'tiles';
    const layer = {
      id: `session_${Date.now()}`, group: 'session', type,
      name: pair, sub: ['排队 · 0%', 'Queued · 0%'], kind: ['自动识别', 'Auto-detect'],
      state: 'loading', progress: 0, visible: true, opacity: 100, shadows: true,
      geo: type === 'imagery' ? 'raster' : 'shared', orderable: type === 'imagery',
      memory: 0, info: { format: ['待识别', 'Detecting'], crs: '—', root: '—', source: [source, source], attribution: ['—', '—'] },
      stats: { visible: 0, resident: 0, requests: 1, failed: 0, cpu: 0, gpu: 0, textures: 0, queued: 1, download: 0, parse: 0, pack: 0, upload: 0 }
    };
    LAYERS.push(layer);
    state.selected = layer.id;
    state.tab = 'source';
    renderLayers();
    renderProps();
    renderDiag();
    toast(`${tx('added')} · ${t(pair)}`);
    setStatus(`${tx('addQueued')} · ${t(pair)}`);
    setTimeout(() => {
      layer.state = 'ready';
      layer.progress = 100;
      layer.memory = 32;
      layer.sub = ['就绪 · 32 MB', 'Ready · 32 MB'];
      layer.stats.resident = 42;
      renderLayers();
      renderDiag();
      if (state.selected === layer.id) renderProps();
      toast(`${t(pair)} · ${tx('addReady')}`);
    }, 1600);
  }

  /* --------------------------------------------------------------- 视点 */
  function saveViewpoint() {
    const c = state.coords;
    const namePair = state.lang === 'en' ? [`Viewpoint ${state.viewIndex}`, `Viewpoint ${state.viewIndex}`] : [`视点 ${state.viewIndex}`, `Viewpoint ${state.viewIndex}`];
    state.points.push({ name: namePair, lon: c.lon, lat: c.lat, h: c.h });
    state.viewIndex += 1;
    renderViewpoints();
    toast(tx('viewpointSaved'));
    setStatus(tx('viewpointSaved'));
  }

  function flyToPoint(point) {
    state.coords = { lon: point.lon, lat: point.lat, h: point.h, heading: state.coords.heading, pitch: state.coords.pitch, distance: state.coords.distance };
    updateHud();
    showFlight(`${tx('flightStart')} · ${point.lon.toFixed(4)}°, ${point.lat.toFixed(4)}°`);
    setStatus(tx('flightStart'));
    toast(tx('flightStart'));
  }

  /* --------------------------------------------------------------- 设置/语言 */
  function setTheme(theme) {
    state.theme = theme;
    document.documentElement.dataset.theme = theme;
    $$('[data-theme-set]').forEach((b) => b.classList.toggle('is-on', b.dataset.themeSet === theme));
  }

  function renderAll() {
    renderLayers();
    renderProps();
    renderDiag();
    renderViewpoints();
    renderChat();
    updateHud();
    const status = $('#statusText');
    if (status && status.textContent === '就绪') status.textContent = tx('statusDefault');
    if (status && status.textContent === 'Ready') status.textContent = tx('statusDefault');
  }

  function setLang(lang) {
    state.lang = lang === 'en' ? 'en' : 'zh';
    document.documentElement.lang = state.lang === 'en' ? 'en' : 'zh-CN';
    $$('[data-lang-set]').forEach((b) => b.classList.toggle('is-on', b.dataset.langSet === state.lang));
    renderAll();
    translateStatic();
    // 动态状态条与提示使用当前语言
    const status = $('#statusText');
    if (status && !statusTimer) status.textContent = tx('statusDefault');
  }

  /* --------------------------------------------------------------- 设备 */
  function setDevice(device) {
    state.device = device;
    const stage = $('#stage');
    stage.dataset.device = device;
    $('#desktopApp').hidden = device !== 'desktop';
    $('#mobileApp').hidden = device !== 'mobile';
    $$('[data-device]').forEach((b) => b.classList.toggle('is-on', b.dataset.device === device));
    closeMobilePanels();
  }

  /* --------------------------------------------------------------- 移动端 */
  function openMobilePanel(which) {
    closeMobilePanels();
    const drawer = $('#mDrawer');
    const scrim = $('#mScrim');
    if (which === 'layers') {
      drawer.classList.add('is-open');
      scrim.hidden = false;
      setMobileNav('layers');
      return;
    }
    if (which === 'ai') {
      $('#mAiPage').classList.add('is-open');
      return;
    }
    const sheet = $(`.m-sheet[data-sheet="${which}"]`);
    if (sheet) {
      sheet.classList.add('is-open');
      scrim.hidden = false;
    }
    if (which === 'props') setMobileNav('props');
    if (which === 'more') setMobileNav('more');
    if (which === 'add') setMobileNav('view');
  }

  function closeMobilePanels() {
    $$('.m-sheet.is-open').forEach((s) => s.classList.remove('is-open'));
    $('#mDrawer')?.classList.remove('is-open');
    $('#mScrim').hidden = true;
    if ($('#mAiPage')) $('#mAiPage').classList.remove('is-open');
    setMobileNav('view');
  }

  function setMobileNav(key) {
    $$('#mNav button').forEach((b) => b.classList.toggle('is-on', b.dataset.nav === key));
  }

  /* --------------------------------------------------------------- 场景 */
  let diagOpenTimer = null;
  function applyScene(scene) {
    state.scene = scene;
    clearTimeout(diagOpenTimer);
    closePopovers();
    closeModals();
    closeScript();
    setDiag(false);
    setRight('props');
    closeMobilePanels();
    const app = $('#desktopApp');
    app.classList.remove('empty-scene');
    setFullscreen(false);
    if (scene === 'script' && state.device === 'desktop') openScript();
    else if (scene === 'diag') {
      if (state.device === 'mobile') openMobilePanel('status');
      else diagOpenTimer = setTimeout(() => setDiag(true), 220);
    } else if (scene === 'add') {
      if (state.device === 'mobile') openMobilePanel('add');
      else openModal('add');
    } else if (scene === 'fly') {
      if (state.device === 'mobile') openMobilePanel('fly');
      else openModal('fly');
    } else if (scene === 'settings') {
      if (state.device === 'mobile') mToast(tx('mobileSettings'));
      else openModal('settings');
    } else if (scene === 'trust') {
      if (state.device === 'mobile') openMobilePanel('more');
      else { openScript(); openModal('trust'); }
    } else if (scene === 'empty') {
      app.classList.add('empty-scene');
      const pick = $('#pickCard');
      if (pick) pick.hidden = true;
    } else if (scene === 'layers') {
      if (state.device === 'mobile') openMobilePanel('layers');
    } else if (scene === 'props') {
      if (state.device === 'mobile') openMobilePanel('props');
    }
    const select = $('#sceneSelect');
    if (select && select.value !== scene) select.value = scene;
  }

  /* --------------------------------------------------------------- 事件 */
  function bindEvents() {
    // 弹层
    $$('[data-pop]').forEach((btn) => {
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        const wrap = btn.closest('.pop-wrap');
        const open = wrap.classList.contains('open');
        closePopovers();
        if (!open) {
          wrap.classList.add('open');
          btn.setAttribute('aria-expanded', 'true');
        }
      });
    });
    document.addEventListener('click', (e) => {
      if (!e.target.closest('.pop-wrap')) closePopovers();
    });

    // 通用打开
    document.addEventListener('click', (e) => {
      const trigger = e.target.closest('[data-open]');
      if (!trigger) return;
      const kind = trigger.dataset.open;
      const source = trigger.dataset.source;
      if (kind === 'add') {
        closePopovers();
        if (state.device === 'mobile') openMobilePanel('add');
        else {
          selectSource(source || 'local');
          openModal('add');
        }
      } else if (kind === 'fly') {
        closePopovers();
        if (state.device === 'mobile') openMobilePanel('fly');
        else openModal('fly');
      } else if (kind === 'settings') {
        closePopovers();
        if (state.device === 'mobile') mToast(tx('mobileSettings'));
        else openModal('settings');
      } else if (kind === 'about') {
        closePopovers();
        if (state.device === 'mobile') mToast(tx('about'));
        else openModal('about');
      } else if (kind === 'quit') {
        closePopovers();
        if (state.device === 'mobile') mToast(tx('quitDone'));
        else openModal('quit');
      } else if (kind === 'ai') {
        closePopovers();
        if (state.device === 'mobile') openMobilePanel('ai');
        else setRight(state.right === 'ai' ? 'props' : 'ai');
      } else if (kind === 'script') {
        closePopovers();
        if (state.device === 'desktop') openScript();
      }
    });

    // 关闭弹窗
    document.addEventListener('click', (e) => {
      const closer = e.target.closest('[data-close]');
      if (closer) closeModals();
    });

    // 图层选择/眼睛/分组
    ['#layerList', '#mLayerList'].forEach((sel) => {
      const list = $(sel);
      if (!list) return;
      list.addEventListener('click', (e) => {
        const eye = e.target.closest('[data-eye]');
        if (eye) {
          e.stopPropagation();
          toggleLayer(eye.dataset.eye);
          return;
        }
        const groupToggle = e.target.closest('[data-group-toggle]');
        if (groupToggle) {
          const id = groupToggle.dataset.groupToggle;
          if (state.collapsedGroups.has(id)) state.collapsedGroups.delete(id);
          else state.collapsedGroups.add(id);
          renderLayers();
          return;
        }
        const row = e.target.closest('[data-layer]');
        if (row) {
          state.selected = row.dataset.layer;
          state.tab = 'source';
          renderLayers();
          renderProps();
          if (sel === '#mLayerList') {
            closeMobilePanels();
            openMobilePanel('props');
          }
        }
      });
    });

    // 属性标签
    $$('#propTabs [data-tab]').forEach((btn) => {
      btn.addEventListener('click', () => {
        state.tab = btn.dataset.tab;
        renderProps();
      });
    });

    // 右上角更多 / 折叠
    document.addEventListener('click', (e) => {
      const act = e.target.closest('[data-act]');
      if (!act) return;
      const name = act.dataset.act;
      if (name === 'collapse-left') {
        state.leftCollapsed = true;
        $('#desktopApp').classList.add('left-collapsed');
      } else if (name === 'open-left') {
        state.leftCollapsed = false;
        $('#desktopApp').classList.remove('left-collapsed');
      } else if (name === 'collapse-right') {
        state.rightCollapsed = true;
        $('#desktopApp').classList.add('right-collapsed');
      } else if (name === 'open-right') {
        state.rightCollapsed = false;
        $('#desktopApp').classList.remove('right-collapsed');
      } else if (name === 'close-ai') {
        setRight('props');
      } else if (name === 'new-chat') {
        state.chat = [{ role: 'assistant', text: ['新聊天已开始。当前场景与视点会保留。', 'New chat started. The current scene and viewpoints are kept.'] }];
        renderChat();
        toast(state.lang === 'en' ? 'New chat' : '新聊天');
      } else if (name === 'close-script') {
        closeScript();
      } else if (name === 'toggle-diag') {
        setDiag(!state.diag);
      } else if (name === 'copy-coord') {
        toast(tx('copyCoord'));
      } else if (name === 'pick-fly') {
        const target = LAYERS.find((l) => l.id === 'buildings');
        openFly(target ? target.pick : null);
      } else if (name === 'copy-pick') {
        toast(tx('copied'));
      } else if (name === 'close-pick') {
        $('#pickCard').hidden = true;
      } else if (name === 'cancel-flight') {
        $('#flightChip').hidden = true;
        setStatus(tx('flightCancel'));
        toast(tx('flightCancel'));
      } else if (name === 'reset-view') {
        state.coords = { lon: 121.4737, lat: 31.2304, h: 8.42, heading: 18, pitch: -35, distance: 12.4 };
        updateHud();
        toast(tx('resetView'));
      } else if (name === 'zoom-in' || name === 'zoom-out') {
        const factor = name === 'zoom-in' ? 0.7 : 1.4;
        state.coords.h = clamp(state.coords.h * factor, 0.05, 20000);
        state.coords.distance = clamp(state.coords.distance * factor, 0.2, 44);
        updateHud();
        toast(tx('zoom'));
      } else if (name === 'save-viewpoint') {
        saveViewpoint();
      } else if (name === 'copy-diag') {
        toast(tx('errorCopy'));
      } else if (name === 'export-diag') {
        toast(tx('diagExport'));
      } else if (name === 'clear-diag') {
        if ($('#errorList')) $('#errorList').innerHTML = `<div class="empty-state"><div class="empty-ic"><svg class="ic"><use href="#i-check"/></svg></div><b>${state.lang === 'en' ? 'No errors' : '没有错误'}</b><p>${tx('diagNote')}</p></div>`;
        toast(tx('diagCleared'));
      } else if (name === 'test-connection') {
        toast(tx('connectionOk'));
      } else if (name === 'clear-context') {
        toast(tx('contextCleared'));
      } else if (name === 'read-capabilities') {
        toast(tx('capsRead'));
      } else if (name === 'fake-pick') {
        toast(tx('filePicked'));
      } else if (name === 'add-data') {
        const active = $('#sourceRail .is-on');
        addLayerFromSource(active ? active.dataset.source : 'local');
        closeModals();
      } else if (name === 'trust-run') {
        state.trusted = true;
        closeModals();
        runScript();
        toast(tx('trustRun'));
      } else if (name === 'reset-scene') {
        state.sceneVersion += 1;
        if ($('#sceneVersion')) $('#sceneVersion').textContent = state.sceneVersion;
        logScript(tx('scriptReset'), 'info');
        toast(tx('scriptReset'));
      } else if (name === 'trust-info') {
        openModal('trust');
      } else if (name === 'toggle-motion') {
        document.body.classList.toggle('reduce-motion');
        act.classList.toggle('is-on');
      } else if (name === 'use-current') {
        const c = state.coords;
        if ($('#flyLon')) $('#flyLon').value = c.lon.toFixed(4);
        if ($('#flyLat')) $('#flyLat').value = c.lat.toFixed(4);
        if ($('#flyHeight')) $('#flyHeight').value = c.h.toFixed(2);
      } else if (name === 'layer-menu') {
        toast(tx('layerMenu'));
      } else if (name === 'confirm-quit') {
        closeModals();
        setStatus(tx('quitDone'));
        toast(tx('quitDone'));
      }
    });

    // 拾取标记
    $('#pickPin')?.addEventListener('click', () => {
      state.selected = 'buildings';
      state.tab = 'pick';
      renderLayers();
      renderProps();
      setRight('props');
      $('#pickCard').hidden = false;
    });
    $('#mPickPin')?.addEventListener('click', () => {
      state.selected = 'buildings';
      state.tab = 'pick';
      renderLayers();
      renderProps();
      openMobilePanel('props');
    });

    // 视点
    document.addEventListener('click', (e) => {
      const go = e.target.closest('[data-vp-go]');
      if (go) {
        const point = state.points[Number(go.dataset.vpGo)];
        if (point) { flyToPoint(point); closePopovers(); }
      }
      const del = e.target.closest('[data-vp-del]');
      if (del) {
        state.points.splice(Number(del.dataset.vpDel), 1);
        renderViewpoints();
        toast(tx('viewpointDeleted'));
      }
    });

    // 添加数据来源
    $$('#sourceRail [data-source]').forEach((btn) => {
      btn.addEventListener('click', () => selectSource(btn.dataset.source));
    });

    // 渲染模式分段
    $$('[data-render]').forEach((btn) => {
      btn.addEventListener('click', () => {
        state.render = btn.dataset.render;
        $$('[data-render]').forEach((b) => b.classList.toggle('is-on', b.dataset.render === state.render));
        toast(`${tx('renderMode' in T ? 'display' : 'display')} · ${state.lang === 'en' ? btn.textContent : btn.textContent}`);
      });
    });

    // 开关（经纬网/地形/阴影）
    $$('[data-toggle]').forEach((btn) => {
      btn.addEventListener('click', (e) => {
        e.stopPropagation();
        const key = btn.dataset.toggle;
        if (key === 'grid') {
          state.grid = !state.grid;
          const grid = LAYERS.find((l) => l.id === 'grid');
          if (grid) grid.visible = state.grid;
          $$('[data-toggle="grid"]').forEach((b) => {
            if (b.classList.contains('switch')) b.classList.toggle('is-on', state.grid);
            else if (b.classList.contains('btn') || b.classList.contains('icon-btn')) {
              b.classList.toggle('is-on', state.grid);
              b.setAttribute('aria-pressed', String(state.grid));
            }
          });
          renderLayers();
          toast(`${tx('经纬网格')} · ${state.grid ? 'ON' : 'OFF'}`);
        } else if (key === 'terrain') {
          state.terrain = !state.terrain;
          $$('[data-toggle="terrain"]').forEach((b) => {
            if (b.classList.contains('switch')) b.classList.toggle('is-on', state.terrain);
            else { b.classList.toggle('is-on', state.terrain); b.setAttribute('aria-pressed', String(state.terrain)); }
          });
          toast(`${tx('地形')} · ${state.terrain ? 'ON' : 'OFF'}`);
        } else if (key === 'shadows') {
          state.shadows = !state.shadows;
          $$('[data-toggle="shadows"]').forEach((b) => b.classList.toggle('is-on', state.shadows));
          toast(`${tx('shadows')} · ${state.shadows ? 'ON' : 'OFF'}`);
        }
      });
    });

    // 相机 / 全屏
    $('#cameraButton')?.addEventListener('click', () => {
      const label = $('#cameraLabel');
      if (!label) return;
      label.textContent = label.textContent === tx('轨道相机') ? tx('飞行相机') : tx('轨道相机');
      setStatus(label.textContent);
    });
    $('#fullButton')?.addEventListener('click', () => setFullscreen(!state.fullscreen));

    // 脚本
    $('#runScript')?.addEventListener('click', runScript);
    $('#stopScript')?.addEventListener('click', stopScript);
    $$('#scTabs [data-sc-tab]').forEach((btn) => btn.addEventListener('click', () => switchScriptTabs(btn.dataset.scTab)));

    // AI 标签
    $$('#aiTabs [data-ai-tab]').forEach((btn) => {
      btn.addEventListener('click', () => {
        state.aiTab = btn.dataset.aiTab;
        $$('#aiTabs [data-ai-tab]').forEach((b) => b.classList.toggle('is-on', b.dataset.aiTab === state.aiTab));
        $$('[data-ai-pane]').forEach((p) => { p.hidden = p.dataset.aiPane !== state.aiTab; });
      });
    });

    // 聊天
    $('#chatSend')?.addEventListener('click', () => sendChat());
    $('#chatInput')?.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); sendChat(); }
    });
    document.addEventListener('click', (e) => {
      const sug = e.target.closest('[data-suggest]');
      if (sug) sendChat(sug.dataset.suggest);
    });
    $('[data-mobile="send-chat"]')?.addEventListener('click', () => {
      const input = $('#mChatInput');
      const value = input ? input.value : '';
      if (value.trim()) { sendChat(value); input.value = ''; }
    });

    // 诊断标签
    $$('#diagTabs [data-diag-tab]').forEach((btn) => {
      btn.addEventListener('click', () => {
        state.diagTab = btn.dataset.diagTab;
        $$('#diagTabs [data-diag-tab]').forEach((b) => b.classList.toggle('is-on', b.dataset.diagTab === state.diagTab));
        $$('[data-diag-pane]').forEach((p) => { p.hidden = p.dataset.diagPane !== state.diagTab; });
      });
    });

    // 错误复制/重试
    document.addEventListener('click', (e) => {
      const copy = e.target.closest('[data-error-copy]');
      if (copy) { toast(tx('errorCopy')); return; }
      const retry = e.target.closest('[data-error-retry]');
      if (retry) { toast(tx('reload')); return; }
    });

    // 飞往表单
    $('#flyForm')?.addEventListener('submit', (e) => {
      e.preventDefault();
      const lon = Number($('#flyLon').value);
      const lat = Number($('#flyLat').value);
      const h = Number($('#flyHeight').value);
      const heading = Number($('#flyHeading').value) || 0;
      const pitch = Number($('#flyPitch').value) || 0;
      const err = $('#flyError');
      let message = '';
      if (!Number.isFinite(lon) || lon < -180 || lon > 180) message = state.lang === 'en' ? 'Longitude must be between −180 and 180.' : '经度必须在 −180 到 180 之间。';
      else if (!Number.isFinite(lat) || lat < -90 || lat > 90) message = state.lang === 'en' ? 'Latitude must be between −90 and 90.' : '纬度必须在 −90 到 90 之间。';
      else if (!Number.isFinite(h) || h < 0 || h > 100000) message = state.lang === 'en' ? 'Height must be between 0 and 100,000 km.' : '观察高度必须在 0 到 100,000 km 之间。';
      if (message) {
        if (err) { err.textContent = message; err.hidden = false; }
        return;
      }
      state.coords = { lon, lat, h, heading, pitch, distance: clamp(3.35 + Math.log(h + 1) * 0.92, 3.4, 44) };
      updateHud();
      closeModals();
      showFlight(`${tx('flightStart')} · ${Math.abs(lon).toFixed(4)}°${lon >= 0 ? 'E' : 'W'}, ${Math.abs(lat).toFixed(4)}°${lat >= 0 ? 'N' : 'S'}`);
      setStatus(tx('flightStart'));
      toast(tx('flightStart'));
    });

    // 移动端动作
    $('#mobileApp')?.addEventListener('click', (e) => {
      const trigger = e.target.closest('[data-mobile]');
      if (!trigger) return;
      const action = trigger.dataset.mobile;
      if (action === 'drawer') openMobilePanel('layers');
      else if (action === 'more') openMobilePanel('more');
      else if (action === 'add') openMobilePanel('add');
      else if (action === 'status') openMobilePanel('status');
      else if (action === 'close') closeMobilePanels();
      else if (action === 'close-page') $('#mAiPage').classList.remove('is-open');
      else if (action === 'ai') openMobilePanel('ai');
      else if (action === 'settings') { closeMobilePanels(); mToast(tx('mobileSettings')); }
      else if (action === 'about') { closeMobilePanels(); mToast(tx('about')); }
      else if (action === 'add-full') { closeMobilePanels(); mToast(tx('filePicked')); }
      else if (action === 'do-fly') {
        const lon = Number($('#mFlyLon').value);
        const lat = Number($('#mFlyLat').value);
        const h = Number($('#mFlyHeight').value);
        if (Number.isFinite(lon) && Number.isFinite(lat) && Number.isFinite(h)) {
          state.coords = { lon, lat, h, heading: Number($('#mFlyHeading').value) || 0, pitch: state.coords.pitch, distance: state.coords.distance };
          updateHud();
          showFlight(tx('flightStart'));
          closeMobilePanels();
          mToast(tx('flightStart'));
        }
      }
    });
    $('[data-mobile="ai"]')?.addEventListener('click', (e) => {
      e.stopPropagation();
      openMobilePanel('ai');
    });

    // 移动端底部导航
    document.addEventListener('click', (e) => {
      const btn = e.target.closest('#mNav button[data-nav]');
      if (!btn) return;
      const nav = btn.dataset.nav;
      if (nav === 'view') closeMobilePanels();
      else if (nav === 'layers') openMobilePanel('layers');
      else if (nav === 'props') openMobilePanel('props');
      else if (nav === 'more') openMobilePanel('more');
    });
    $('#mScrim')?.addEventListener('click', closeMobilePanels);

    // 语言/主题/设备
    $$('[data-lang-set]').forEach((btn) => btn.addEventListener('click', () => setLang(btn.dataset.langSet)));
    $$('[data-theme-set]').forEach((btn) => btn.addEventListener('click', () => setTheme(btn.dataset.themeSet)));
    $$('[data-device]').forEach((btn) => btn.addEventListener('click', () => {
      setDevice(btn.dataset.device);
      applyScene(state.scene);
    }));

    // 场景
    $('#sceneSelect')?.addEventListener('change', (e) => applyScene(e.target.value));

    // 键盘
    document.addEventListener('keydown', (e) => {
      if (e.key === 'Escape') {
        if ($('.modal.is-open')) { closeModals(); return; }
        if ($('#desktopApp').dataset.script === '1') { closeScript(); return; }
        closePopovers();
        closeMobilePanels();
        if (state.fullscreen) setFullscreen(false);
        return;
      }
      const typing = ['INPUT', 'TEXTAREA', 'SELECT'].includes(document.activeElement?.tagName);
      if (typing) return;
      if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase() === 'l') { e.preventDefault(); $('#desktopApp').classList.toggle('left-collapsed'); }
      if (e.ctrlKey && !e.shiftKey && e.key.toLowerCase() === 'p') { e.preventDefault(); $('#desktopApp').classList.toggle('right-collapsed'); }
      if (e.ctrlKey && e.shiftKey && e.key.toLowerCase() === 'f') { e.preventDefault(); setFullscreen(!state.fullscreen); }
      if (e.ctrlKey && e.key.toLowerCase() === 'g') { e.preventDefault(); if (state.device === 'mobile') openMobilePanel('fly'); else openModal('fly'); }
      if (e.ctrlKey && e.key.toLowerCase() === 'o') { e.preventDefault(); if (state.device === 'mobile') openMobilePanel('add'); else openModal('add'); }
      if (!e.ctrlKey && e.key.toLowerCase() === 'f') { handlePropAction('focus'); }
    });

    // 面板拖宽
    $$('[data-resize]').forEach((handle) => {
      handle.addEventListener('pointerdown', (e) => {
        e.preventDefault();
        const app = $('#desktopApp');
        const side = handle.dataset.resize;
        const startX = e.clientX;
        const start = parseFloat(getComputedStyle(app).getPropertyValue(side === 'left' ? '--left' : '--right')) || (side === 'left' ? 264 : 320);
        handle.classList.add('dragging');
        const move = (ev) => {
          const delta = side === 'left' ? ev.clientX - startX : startX - ev.clientX;
          const width = clamp(start + delta, side === 'left' ? 220 : 280, side === 'left' ? 360 : 480);
          app.style.setProperty(side === 'left' ? '--left' : '--right', `${width}px`);
        };
        const up = () => {
          handle.classList.remove('dragging');
          document.removeEventListener('pointermove', move);
          document.removeEventListener('pointerup', up);
        };
        document.addEventListener('pointermove', move);
        document.addEventListener('pointerup', up);
      });
    });

    // 场景空态样式（在 script 标签外由 CSS 支持）
    const style = document.createElement('style');
    style.textContent = `
      .app.empty-scene .globe-city, .app.empty-scene .pick-card,
      .app.empty-scene .pin, .app.empty-scene .tile-chip { display: none !important; }
      .reduce-motion *, .reduce-motion *::before, .reduce-motion *::after { animation: none !important; transition: none !important; }
      .form-row.three { grid-template-columns: repeat(3, minmax(0, 1fr)); }
      .props-scroll .field { margin-bottom: 8px; }
      .props-scroll .field input, .props-scroll .field select { height: 30px; font-size: 11px; }
      .props-scroll select.compact.wide { width: 100%; height: 32px; margin-bottom: 8px; }
      .m-prop-head { display: flex; align-items: center; gap: 9px; padding: 8px 0 10px; border-bottom: 1px solid var(--line); margin-bottom: 10px; }
      .m-prop-head b { display: block; font-size: 13px; font-weight: 600; }
      .m-prop-head span { display: block; color: var(--text-3); font-size: 10px; }
      .m-sheet .form-row.two { grid-template-columns: 1fr 1fr; }
      .m-sheet .field input { height: 40px; }
      .m-sheet .prop-actions .btn { height: 38px; }
      .m-page .msg .bubble { font-size: 12px; }
      .m-page .chat-log { padding-bottom: 6px; }
    `;
    document.head.appendChild(style);
  }

  /* ------------------------------------------------------------------ 初始化 */
  function init() {
    const params = new URLSearchParams(location.search);
    if (params.get('shot') === '1') document.body.classList.add('shot');
    const lang = params.get('lang') === 'en' ? 'en' : 'zh';
    const theme = params.get('theme') === 'light' ? 'light' : 'dark';
    const device = 'mobile';
    setTheme(theme);
    setDevice(device);
    setLang(lang);
    bindEvents();
    renderAll();
    translateStatic();
    const scene = params.get('scene') || 'main';
    applyScene(scene);
    const c = state.coords;
    if ($('#flyLon')) $('#flyLon').value = c.lon.toFixed(4);
    if ($('#flyLat')) $('#flyLat').value = c.lat.toFixed(4);
    if ($('#flyHeight')) $('#flyHeight').value = c.h.toFixed(2);
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', init);
  else init();
})();
