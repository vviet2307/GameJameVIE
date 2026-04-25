/* ============================================================
   game.js — Au fil de la vie (The Course of Life)
   Temple-Run style Endless Runner · Three.js r150
   ============================================================ */

import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';

// ============================================================
// 1. CONSTANTS
// ============================================================

const LANES        = [-2.5, 0, 2.5];   // lane X positions
const ROAD_WIDTH   = 8;
const SEG_DEPTH    = 25;
const SEG_COUNT    = 7;
const SPAWN_DIST   = 85;
const DESPAWN_DIST = 14;

const GRAVITY       = -24;
const JUMP_VELOCITY = 13;
const PLAYER_BASE_Y = 0.55;

/**
 * HOW TO ADD YOUR OWN MODELS
 * ─────────────────────────────────────────────────────────────
 * 1. Create a  models/  folder next to index.html.
 * 2. Export your assets as .glb (binary GLTF) from Blender /
 *    any 3D tool.
 * 3. Drop them in models/ with exactly these filenames:
 *      models/kirikou_baby.glb
 *      models/kirikou_child.glb
 *      models/kirikou_adult.glb
 *      models/poupette.glb        ← obstacle
 *      models/karaba.glb          ← boss
 * 4. If a file is missing the game falls back to the coloured
 *    primitive automatically — no crash.
 *
 * SCALE / PIVOT TIPS
 * ─────────────────────────────────────────────────────────────
 * • In Blender, Apply All Transforms before exporting.
 * • Set the mesh origin at the model's feet (not its centre).
 * • Export at scale 1 — tweak MODEL_SCALE_* below if needed.
 */
const MODEL_SCALE_PLAYER  = 1.0;   // overall scale for Kirikou models
const MODEL_SCALE_POUPETTE = 1.0;  // scale for Poupette model
const MODEL_SCALE_KARABA   = 1.0;  // scale for Karaba model

/**
 * PHASE TABLE
 * speed   — world scroll speed in units/s (increase = faster game)
 * scale   — uniform scale of the player group
 * hitRadius — collision detection radius (tune to match your model)
 */
const PHASES = [
  {
    name:      'Bébé',
    minScore:  0,
    maxScore:  500,
    speed:     11,          // was 8
    scale:     0.38,
    hitRadius: 0.44,
    color:     0x66BB6A,
    shape:     'sphere',
    modelPath: 'models/kirikou_baby.glb',
    model:     null,        // filled by preloadAssets()
  },
  {
    name:      'Enfant',
    minScore:  500,
    maxScore:  1500,
    speed:     17,          // was 13
    scale:     0.62,
    hitRadius: 0.66,
    color:     0x42A5F5,
    shape:     'cylinder',
    modelPath: 'models/kirikou_child.glb',
    model:     null,
  },
  {
    name:      'Adulte',
    minScore:  1500,
    maxScore:  Infinity,
    speed:     24,          // was 19
    scale:     0.86,
    hitRadius: 0.88,
    color:     0xEF5350,
    shape:     'box',
    modelPath: 'models/kirikou_adult.glb',
    model:     null,
  },
];

// Loaded GLTF scenes for obstacles and boss
const assets = { poupette: null, karaba: null };

// ============================================================
// 2. ASSET LOADING
// ============================================================

const gltfLoader = new GLTFLoader();

/**
 * Tries to load a .glb file. Resolves with the scene on success,
 * null on any error (missing file, wrong format, etc.).
 */
function tryLoad(path) {
  return new Promise(resolve => {
    gltfLoader.load(
      path,
      gltf  => resolve(gltf.scene),
      // progress — update the loading bar
      xhr   => setLoadingBar(xhr.loaded / (xhr.total || 1)),
      _err  => resolve(null),    // fail gracefully, use primitive
    );
  });
}

function setLoadingBar(fraction) {
  const el = document.getElementById('loadingFill');
  if (el) el.style.width = `${Math.round(fraction * 100)}%`;
}

async function preloadAssets() {
  // All 5 fetches run in parallel; failures return null
  const paths = [
    ...PHASES.map(p => p.modelPath),
    'models/poupette.glb',
    'models/karaba.glb',
  ];

  const results = await Promise.all(paths.map(tryLoad));

  PHASES[0].model = results[0];
  PHASES[1].model = results[1];
  PHASES[2].model = results[2];
  assets.poupette  = results[3];
  assets.karaba    = results[4];

  setLoadingBar(1);
}

// ============================================================
// 3. MUTABLE GAME STATE
// ============================================================

let gameState    = 'menu';
let score        = 0;
let distance     = 0;
let highScore    = 0;
let phaseIndex   = 0;
let strikes      = 0;
let isInvincible = false;
let invTimer     = 0;
let worldSpeed   = PHASES[0].speed;
let targetScale  = PHASES[0].scale;

let playerLane  = 1;
let playerX     = LANES[1];
let playerY     = PLAYER_BASE_Y;
let playerVelY  = 0;
let isJumping   = false;

let laneChanging = false;
let laneFromX    = LANES[1];
let laneToX      = LANES[1];
let laneT        = 0;

let obstacleTimer    = 0;
let obstacleInterval = 1.9;   // was 2.2 — slightly tighter from the start

let shakeTime     = 0;
let shakeDuration = 0;

// ============================================================
// 4. RENDERER & SCENE
// ============================================================

const canvas   = document.getElementById('gameCanvas');
const renderer = new THREE.WebGLRenderer({ canvas, antialias: true });
renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
renderer.shadowMap.enabled = true;
renderer.shadowMap.type    = THREE.PCFSoftShadowMap;

const scene = new THREE.Scene();
scene.background = new THREE.Color(0x87CEEB);
scene.fog        = new THREE.Fog(0x87CEEB, 55, 140);

const camera = new THREE.PerspectiveCamera(78, 1, 0.1, 200);
camera.position.set(0, 5.5, 10);
const CAM_BASE = new THREE.Vector3(0, 5.5, 10);

// ============================================================
// 5. LIGHTING
// ============================================================

scene.add(new THREE.AmbientLight(0xffffff, 0.55));

const sun = new THREE.DirectionalLight(0xffeedd, 0.9);
sun.position.set(10, 30, 20);
sun.castShadow          = true;
sun.shadow.mapSize.set(1024, 1024);
sun.shadow.camera.left   = -30;
sun.shadow.camera.right  =  30;
sun.shadow.camera.top    =  30;
sun.shadow.camera.bottom = -30;
scene.add(sun);

scene.add(new THREE.HemisphereLight(0x87CEEB, 0x4CAF50, 0.35));

// ============================================================
// 6. ROAD SYSTEM
// ============================================================

const roadMat = new THREE.MeshLambertMaterial({ color: 0x546E7A });
const roadGeo = new THREE.BoxGeometry(ROAD_WIDTH, 0.3, SEG_DEPTH);
const curbMat = new THREE.MeshLambertMaterial({ color: 0xECEFF1 });
const curbGeo = new THREE.BoxGeometry(0.45, 0.5, SEG_DEPTH);
const divMat  = new THREE.MeshLambertMaterial({ color: 0xFFFFFF });
const divGeo  = new THREE.BoxGeometry(0.09, 0.34, SEG_DEPTH);

const roadSegments = [];

function createSegment(z) {
  const g = new THREE.Group();
  g.position.z = z;

  const road = new THREE.Mesh(roadGeo, roadMat);
  road.position.y = -0.15;
  road.receiveShadow = true;
  g.add(road);

  for (const side of [-1, 1]) {
    const curb = new THREE.Mesh(curbGeo, curbMat);
    curb.position.set(side * (ROAD_WIDTH / 2 + 0.225), -0.1, 0);
    g.add(curb);
  }

  for (const dx of [-1.25, 1.25]) {
    const div = new THREE.Mesh(divGeo, divMat);
    div.position.set(dx, 0.02, 0);
    g.add(div);
  }

  scene.add(g);
  return g;
}

function initRoad() {
  for (let i = 0; i < SEG_COUNT; i++) {
    roadSegments.push(createSegment(-i * SEG_DEPTH));
  }
}

function updateRoad(dt) {
  const step = worldSpeed * dt;
  let minZ = Infinity;

  for (const seg of roadSegments) {
    seg.position.z += step;
    if (seg.position.z < minZ) minZ = seg.position.z;
  }
  for (const seg of roadSegments) {
    if (seg.position.z > DESPAWN_DIST + SEG_DEPTH) {
      seg.position.z = minZ - SEG_DEPTH;
    }
  }
}

// ============================================================
// 7. BACKGROUND SCENERY
// ============================================================

function buildBackground() {
  const grass = new THREE.Mesh(
    new THREE.PlaneGeometry(300, 600),
    new THREE.MeshLambertMaterial({ color: 0x4CAF50 }),
  );
  grass.rotation.x = -Math.PI / 2;
  grass.position.y = -0.31;
  scene.add(grass);

  for (let i = 0; i < 32; i++) {
    const side = i % 2 === 0 ? -1 : 1;
    addTree(side * (ROAD_WIDTH / 2 + 2 + Math.random() * 18), -10 - Math.random() * 200);
  }
}

function addTree(x, z) {
  const g = new THREE.Group();
  g.position.set(x, 0, z);

  const trunk = new THREE.Mesh(
    new THREE.CylinderGeometry(0.18, 0.28, 2.2, 6),
    new THREE.MeshLambertMaterial({ color: 0x795548 }),
  );
  trunk.position.y = 1.1;
  trunk.castShadow = true;
  g.add(trunk);

  for (let i = 0; i < 2; i++) {
    const cone = new THREE.Mesh(
      new THREE.ConeGeometry(1.4 - i * 0.3, 2.4, 7),
      new THREE.MeshLambertMaterial({ color: i === 0 ? 0x2E7D32 : 0x388E3C }),
    );
    cone.position.y = 3.0 + i * 1.5;
    cone.castShadow = true;
    g.add(cone);
  }

  scene.add(g);
}

// ============================================================
// 8. PLAYER — Kirikou
// ============================================================

const playerGroup = new THREE.Group();
scene.add(playerGroup);
let playerMesh = null;

/**
 * Returns either the loaded GLTF model for this phase,
 * or a coloured primitive as fallback.
 */
function buildPlayerMesh(phase) {
  // ── GLTF path ──────────────────────────────────────────────
  if (phase.model) {
    const m = phase.model.clone();
    m.scale.setScalar(MODEL_SCALE_PLAYER);
    m.traverse(n => {
      if (n.isMesh) {
        n.castShadow    = true;
        n.receiveShadow = false;
      }
    });
    return m;
  }

  // ── Primitive fallback ──────────────────────────────────────
  let geo;
  switch (phase.shape) {
    case 'sphere':
      geo = new THREE.SphereGeometry(0.65, 14, 14); break;
    case 'cylinder':
      geo = new THREE.CylinderGeometry(0.45, 0.45, 1.4, 12); break;
    default:
      geo = new THREE.BoxGeometry(0.9, 1.4, 0.9);
  }
  const mesh = new THREE.Mesh(
    geo,
    new THREE.MeshLambertMaterial({ color: phase.color }),
  );
  mesh.castShadow = true;
  return mesh;
}

function setPlayerPhase(phase) {
  if (playerMesh) playerGroup.remove(playerMesh);
  playerMesh = buildPlayerMesh(phase);
  playerGroup.add(playerMesh);
}

function initPlayer() {
  setPlayerPhase(PHASES[phaseIndex]);
  playerGroup.scale.setScalar(PHASES[phaseIndex].scale);
  playerGroup.position.set(LANES[1], PLAYER_BASE_Y, 0);
}

function updatePlayer(dt) {
  if (laneChanging) {
    laneT += dt * 9;
    if (laneT >= 1) { laneT = 1; laneChanging = false; }
    const t = laneT < 0.5
      ? 2 * laneT * laneT
      : -1 + (4 - 2 * laneT) * laneT;
    playerX = laneFromX + (laneToX - laneFromX) * t;
  }

  if (isJumping) {
    playerVelY += GRAVITY * dt;
    playerY    += playerVelY * dt;
    if (playerY <= PLAYER_BASE_Y) {
      playerY = PLAYER_BASE_Y; playerVelY = 0; isJumping = false;
    }
  } else {
    playerY = PLAYER_BASE_Y + Math.sin(Date.now() * 0.004) * 0.04;
  }

  playerGroup.position.set(playerX, playerY, 0);

  const targetLean = laneChanging ? (laneFromX - laneToX) * 0.13 : 0;
  playerGroup.rotation.z = THREE.MathUtils.lerp(
    playerGroup.rotation.z, targetLean, dt * 8,
  );

  if (isInvincible) {
    invTimer -= dt;
    if (playerMesh) playerMesh.visible = Math.floor(invTimer * 10) % 2 === 0;
    if (invTimer <= 0) {
      isInvincible = false;
      if (playerMesh) playerMesh.visible = true;
    }
  }
}

function updatePlayerScale(dt) {
  const cur = playerGroup.scale.x;
  if (Math.abs(cur - targetScale) > 0.002) {
    playerGroup.scale.setScalar(cur + (targetScale - cur) * Math.min(1, dt * 7));
  }
}

// ============================================================
// 9. OBSTACLES — Poupettes
// ============================================================

const obstacles    = [];
const obstaclePool = [];

/**
 * Returns either the loaded GLTF Poupette model or a cone.
 * Models from the pool are raw meshes/groups — we just reposition them.
 */
function getObstacleMesh() {
  if (obstaclePool.length > 0) return obstaclePool.pop();

  // ── GLTF path ──────────────────────────────────────────────
  if (assets.poupette) {
    const m = assets.poupette.clone();
    m.scale.setScalar(MODEL_SCALE_POUPETTE);
    m.traverse(n => { if (n.isMesh) n.castShadow = true; });
    return m;
  }

  // ── Primitive fallback ──────────────────────────────────────
  const mesh = new THREE.Mesh(
    new THREE.ConeGeometry(0.55, 1.3, 8),
    new THREE.MeshLambertMaterial({ color: 0x1a1a1a }),
  );
  mesh.castShadow = true;
  return mesh;
}

function spawnObstacle() {
  const blockCount = Math.random() < 0.22 ? 2 : 1;
  const shuffled   = [0, 1, 2].sort(() => Math.random() - 0.5);

  for (let i = 0; i < blockCount; i++) {
    const lane = shuffled[i];
    const mesh = getObstacleMesh();
    mesh.position.set(LANES[lane], 0.65, -SPAWN_DIST);
    mesh.rotation.set(0, 0, 0);
    mesh.userData = {
      isDrifting: Math.random() < 0.22,
      driftDir:   Math.random() < 0.5 ? 1 : -1,
      driftSpeed: 1.2 + Math.random() * 1.6,
    };
    scene.add(mesh);
    obstacles.push(mesh);
  }
}

function updateObstacles(dt) {
  obstacleTimer += dt;
  if (obstacleTimer >= obstacleInterval) {
    obstacleTimer    = 0;
    obstacleInterval = Math.max(0.60, 1.9 - score / 2400);
    spawnObstacle();
  }

  for (let i = obstacles.length - 1; i >= 0; i--) {
    const obs = obstacles[i];
    obs.position.z += worldSpeed * dt;
    obs.rotation.y += dt * 2.5;

    if (obs.userData.isDrifting) {
      obs.position.x += obs.userData.driftDir * obs.userData.driftSpeed * dt;
      const limit = ROAD_WIDTH / 2 - 0.7;
      if (Math.abs(obs.position.x) > limit) {
        obs.userData.driftDir *= -1;
        obs.position.x = Math.sign(obs.position.x) * limit;
      }
    }

    if (obs.position.z > DESPAWN_DIST) {
      scene.remove(obs);
      obstaclePool.push(obs);
      obstacles.splice(i, 1);
    }
  }
}

// ============================================================
// 10. KARABA — boss chaser
// ============================================================

const karabaGroup = new THREE.Group();
karabaGroup.position.set(0, 1.4, -28);
scene.add(karabaGroup);

/**
 * Called once after preloadAssets() — uses GLTF if available,
 * otherwise builds the primitive cylinder + eyes + horns.
 */
function buildKaraba() {
  // Clear any previous children (safe to call on restart too)
  while (karabaGroup.children.length > 0) {
    karabaGroup.remove(karabaGroup.children[0]);
  }

  // ── GLTF path ──────────────────────────────────────────────
  if (assets.karaba) {
    const m = assets.karaba.clone();
    m.scale.setScalar(MODEL_SCALE_KARABA);
    m.traverse(n => { if (n.isMesh) n.castShadow = true; });
    karabaGroup.add(m);
    return;
  }

  // ── Primitive fallback ──────────────────────────────────────
  const body = new THREE.Mesh(
    new THREE.CylinderGeometry(0.75, 1.0, 2.9, 10),
    new THREE.MeshLambertMaterial({ color: 0xCC0000 }),
  );
  body.castShadow = true;
  karabaGroup.add(body);

  for (const ex of [-0.32, 0.32]) {
    const eye = new THREE.Mesh(
      new THREE.SphereGeometry(0.13, 8, 8),
      new THREE.MeshBasicMaterial({ color: 0xFFFF00 }),
    );
    eye.position.set(ex, 1.0, 0.7);
    karabaGroup.add(eye);
  }

  for (const hx of [-0.45, 0.45]) {
    const horn = new THREE.Mesh(
      new THREE.ConeGeometry(0.13, 0.75, 6),
      new THREE.MeshLambertMaterial({ color: 0x880000 }),
    );
    horn.position.set(hx, 1.9, 0);
    horn.rotation.z = hx > 0 ? -0.35 : 0.35;
    karabaGroup.add(horn);
  }
}

function updateKaraba(dt) {
  const targetZ   = strikes === 0 ? -22 : -8;
  const lerpSpeed = strikes === 0 ? 0.45 : 2.8;
  karabaGroup.position.z = THREE.MathUtils.lerp(
    karabaGroup.position.z, targetZ, dt * lerpSpeed,
  );
  karabaGroup.position.y = 1.4 + Math.sin(Date.now() * 0.0028) * 0.32;
  karabaGroup.rotation.y += dt * 1.6;
  karabaGroup.rotation.z  = Math.sin(Date.now() * 0.004) * 0.07;
}

// ============================================================
// 11. PARTICLES  (phase transition burst)
// ============================================================

const particles = [];

function spawnParticles(pos, color) {
  for (let i = 0; i < 24; i++) {
    const p = new THREE.Mesh(
      new THREE.SphereGeometry(0.13, 5, 5),
      new THREE.MeshBasicMaterial({ color, transparent: true, opacity: 1 }),
    );
    p.position.copy(pos);
    p.userData = {
      vel:  new THREE.Vector3(
        (Math.random() - 0.5) * 7,
        2 + Math.random() * 7,
        (Math.random() - 0.5) * 7,
      ),
      life: 1.0,
    };
    scene.add(p);
    particles.push(p);
  }
}

function updateParticles(dt) {
  for (let i = particles.length - 1; i >= 0; i--) {
    const p = particles[i];
    p.userData.life -= dt * 1.7;
    if (p.userData.life <= 0) { scene.remove(p); particles.splice(i, 1); continue; }
    p.position.addScaledVector(p.userData.vel, dt);
    p.userData.vel.y += GRAVITY * 0.22 * dt;
    const l = p.userData.life;
    p.scale.setScalar(l);
    p.material.opacity = l;
  }
}

// ============================================================
// 12. PHASE MANAGEMENT
// ============================================================

function phaseForScore(s) {
  for (let i = PHASES.length - 1; i >= 0; i--) {
    if (s >= PHASES[i].minScore) return i;
  }
  return 0;
}

function tickPhase() {
  const newIndex = phaseForScore(score);
  if (newIndex === phaseIndex) return;
  phaseIndex  = newIndex;
  const phase = PHASES[phaseIndex];
  worldSpeed  = phase.speed;
  targetScale = phase.scale;
  playerGroup.scale.setScalar(0.05);   // pop-then-grow animation
  setPlayerPhase(phase);
  spawnParticles(playerGroup.position.clone(), phase.color);
  showPhaseNotif(phase.name);
}

// ============================================================
// 13. COLLISION DETECTION
// ============================================================

function checkCollisions() {
  if (isInvincible || gameState !== 'playing') return;

  const phase = PHASES[phaseIndex];
  const hr    = phase.hitRadius * phase.scale;

  for (let i = obstacles.length - 1; i >= 0; i--) {
    const obs = obstacles[i];
    if (obs.position.z < -2.5 || obs.position.z > 3.5) continue;

    const dx = Math.abs(obs.position.x - playerX);
    const dz = Math.abs(obs.position.z);

    if (isJumping && playerY > PLAYER_BASE_Y + 1.6) continue;

    if (dx < hr + 0.42 && dz < hr + 0.55) {
      scene.remove(obs);
      obstaclePool.push(obs);
      obstacles.splice(i, 1);
      handleHit();
      return;
    }
  }
}

function handleHit() {
  strikes++;
  if (strikes >= 2) { doGameOver(); return; }
  isInvincible = true;
  invTimer     = 2.2;
  shakeCamera(0.32);
  document.getElementById('warning').style.display = 'block';
}

// ============================================================
// 14. CAMERA
// ============================================================

function shakeCamera(duration) { shakeTime = shakeDuration = duration; }

function updateCamera(dt) {
  if (shakeTime > 0) {
    shakeTime -= dt;
    const mag = (shakeTime / shakeDuration) * 0.28;
    camera.position.set(
      CAM_BASE.x + (Math.random() - 0.5) * mag,
      CAM_BASE.y + (Math.random() - 0.5) * mag,
      CAM_BASE.z + (Math.random() - 0.5) * mag,
    );
  } else {
    camera.position.copy(CAM_BASE);
  }
  camera.lookAt(playerX * 0.35, 0.5, -7);
}

// ============================================================
// 15. SCORE & HUD
// ============================================================

function updateScore(dt) {
  distance += worldSpeed * dt;
  score     = Math.floor(distance * 0.2);
  document.getElementById('score').textContent      = score;
  document.getElementById('phaseLabel').textContent = PHASES[phaseIndex].name;
}

function showPhaseNotif(name) {
  const el = document.getElementById('phaseNotif');
  el.textContent = `✦ ${name.toUpperCase()} ✦`;
  el.classList.add('show');
  setTimeout(() => el.classList.remove('show'), 2200);
}

function doGameOver() {
  gameState = 'gameover';
  if (score > highScore) highScore = score;
  document.getElementById('finalScore').textContent        = score;
  document.getElementById('highScore').textContent         = highScore;
  document.getElementById('gameoverScreen').style.display  = 'flex';
  document.getElementById('hud').style.display             = 'none';
}

// ============================================================
// 16. CONTROLS
// ============================================================

function changeLane(dir) {
  if (laneChanging) return;
  const next = Math.max(0, Math.min(2, playerLane + dir));
  if (next === playerLane) return;
  laneFromX = playerX; laneToX = LANES[next];
  playerLane = next; laneChanging = true; laneT = 0;
}

function doJump() {
  if (isJumping) return;
  isJumping = true; playerVelY = JUMP_VELOCITY;
}

window.addEventListener('keydown', (e) => {
  if (gameState !== 'playing') {
    if (e.code === 'Space' || e.code === 'Enter' || e.code === 'KeyR') startGame();
    return;
  }
  switch (e.code) {
    case 'ArrowLeft':  changeLane(-1); break;
    case 'ArrowRight': changeLane(1);  break;
    case 'ArrowUp':
    case 'Space':      doJump();       break;
  }
  e.preventDefault();
});

let touchX0 = 0, touchY0 = 0;
window.addEventListener('touchstart', (e) => {
  touchX0 = e.touches[0].clientX; touchY0 = e.touches[0].clientY;
  e.preventDefault();
}, { passive: false });

window.addEventListener('touchend', (e) => {
  if (gameState !== 'playing') { startGame(); return; }
  const dx = e.changedTouches[0].clientX - touchX0;
  const dy = e.changedTouches[0].clientY - touchY0;
  const T  = 28;
  if (Math.abs(dx) > Math.abs(dy) && Math.abs(dx) > T) changeLane(dx > 0 ? 1 : -1);
  else if (dy < -T) doJump();
  e.preventDefault();
}, { passive: false });

document.getElementById('playBtn').addEventListener('click',  startGame);
document.getElementById('retryBtn').addEventListener('click', startGame);

// ============================================================
// 17. GAME LIFECYCLE
// ============================================================

function startGame() {
  document.getElementById('menuScreen').style.display     = 'none';
  document.getElementById('gameoverScreen').style.display = 'none';
  document.getElementById('hud').style.display            = 'flex';
  document.getElementById('warning').style.display        = 'none';

  gameState = 'playing'; score = 0; distance = 0;
  phaseIndex = 0; strikes = 0;
  isInvincible = false; invTimer = 0;
  worldSpeed = PHASES[0].speed; targetScale = PHASES[0].scale;

  playerLane = 1; playerX = LANES[1]; playerY = PLAYER_BASE_Y;
  playerVelY = 0; isJumping = false; laneChanging = false;
  obstacleTimer = 0; obstacleInterval = 1.9; shakeTime = 0;

  for (const obs of obstacles) scene.remove(obs);
  obstacles.length = 0;
  for (const p of particles) scene.remove(p);
  particles.length = 0;

  karabaGroup.position.set(0, 1.4, -28);

  setPlayerPhase(PHASES[0]);
  playerGroup.scale.setScalar(PHASES[0].scale);
  playerGroup.position.set(LANES[1], PLAYER_BASE_Y, 0);
  playerGroup.rotation.set(0, 0, 0);
}

function showMenu() {
  document.getElementById('menuScreen').style.display     = 'flex';
  document.getElementById('gameoverScreen').style.display = 'none';
  document.getElementById('hud').style.display            = 'none';
}

function showLoading(visible) {
  document.getElementById('loadingScreen').style.display = visible ? 'flex' : 'none';
}

// ============================================================
// 18. MAIN LOOP
// ============================================================

let prevTime = 0;

function loop(timestamp) {
  requestAnimationFrame(loop);
  const dt = Math.min((timestamp - prevTime) / 1000, 0.05);
  prevTime = timestamp;

  if (gameState === 'playing') {
    updateRoad(dt);
    updateObstacles(dt);
    updateKaraba(dt);
    updatePlayer(dt);
    updatePlayerScale(dt);
    checkCollisions();
    updateScore(dt);
    tickPhase();
    updateParticles(dt);
    updateCamera(dt);
  }

  renderer.render(scene, camera);
}

// ============================================================
// 19. RESIZE
// ============================================================

function onResize() {
  const container = document.getElementById('container');
  const w = container.clientWidth;
  const h = container.clientHeight;
  renderer.setSize(w, h, false);
  camera.aspect = w / h;
  camera.fov    = w < h ? 82 : 65;
  camera.updateProjectionMatrix();
}

window.addEventListener('resize', onResize);

// ============================================================
// 20. BOOT  (async — waits for GLTF preload before starting)
// ============================================================

async function boot() {
  onResize();
  showLoading(true);

  await preloadAssets();   // ~instant if models/ folder is empty

  buildKaraba();           // swap primitive → model if loaded
  initRoad();
  buildBackground();
  initPlayer();

  showLoading(false);
  showMenu();
  requestAnimationFrame(loop);
}

boot();
