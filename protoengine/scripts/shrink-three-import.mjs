import fs from 'fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const indexPath = path.resolve(__dirname, '..', 'index.js');

let s = fs.readFileSync(indexPath, 'utf8');

const names = [
  'MeshStandardMaterial',
  'MeshBasicMaterial',
  'Float32BufferAttribute',
  'BufferGeometry',
  'BufferAttribute',
  'PerspectiveCamera',
  'ExtrudeGeometry',
  'CapsuleGeometry',
  'CylinderGeometry',
  'PlaneGeometry',
  'TextureLoader',
  'LoadingManager',
  'WebGLRenderer',
  'PCFSoftShadowMap',
  'ACESFilmicToneMapping',
  'SRGBColorSpace',
  'RepeatWrapping',
  'DoubleSide',
  'FrontSide',
  'AmbientLight',
  'PointLight',
  'Raycaster',
  'FileLoader',
  'Quaternion',
  'Matrix4',
  'Matrix3',
  'Vector3',
  'Vector2',
  'BoxGeometry',
  'Scene',
  'Group',
  'Shape',
  'Path',
  'Color',
  'Euler',
  'Clock',
  'Box3',
  'Mesh',
  'MathUtils'
];

for (const n of names) {
  s = s.replaceAll(`THREE.${n}`, n);
}

const sorted = names
  .filter((n) => n !== 'MathUtils')
  .sort((a, b) => a.localeCompare(b));
sorted.push('MathUtils');

const newImp = `import {\n    ${sorted.join(',\n    ')}\n} from 'three';`;
const oldImp = "import * as THREE from 'three';";

if (!s.includes(oldImp)) {
  throw new Error('Expected namespace import line missing — already converted?');
}

s = s.replace(oldImp, newImp);
fs.writeFileSync(indexPath, s);
console.log('Updated', indexPath);
