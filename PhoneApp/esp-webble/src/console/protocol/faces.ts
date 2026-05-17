import type { FaceId, ShapeId, Vec3 } from "./types";

export function faceNormal(f: FaceId): Vec3 {
  switch (f) {
    case "TOP":
      return { x: 0, y: 0, z: 1 };
    case "BOTTOM":
      return { x: 0, y: 0, z: -1 };
    case "FRONT":
      return { x: 1, y: 0, z: 0 };
    case "BACK":
      return { x: -1, y: 0, z: 0 };
    case "LEFT":
      return { x: 0, y: 1, z: 0 };
    case "RIGHT":
      return { x: 0, y: -1, z: 0 };
  }
}

export function faceBasis(f: FaceId): { up: Vec3; right: Vec3 } {
  switch (f) {
    case "TOP":
      return {
        up: { x: 0, y: 1, z: 0 },
        right: { x: 1, y: 0, z: 0 },
      };

    case "BOTTOM":
      return {
        up: { x: 0, y: -1, z: 0 },
        right: { x: -1, y: 0, z: 0 },
      };

    case "FRONT":
      return {
        up: { x: 0, y: 0, z: 1 },
        right: { x: 0, y: -1, z: 0 },
      };

    case "BACK":
      return {
        up: { x: 0, y: 0, z: 1 },
        right: { x: 0, y: 1, z: 0 },
      };

    case "LEFT":
      return {
        up: { x: 0, y: 0, z: 1 },
        right: { x: 1, y: 0, z: 0 },
      };

    case "RIGHT":
      return {
        up: { x: 0, y: 0, z: 1 },
        right: { x: -1, y: 0, z: 0 },
      };
  }
}

export function arrowFromTo(from: FaceId, to: FaceId): ShapeId {
  const n = faceNormal(to);
  const { up, right } = faceBasis(from);

  const du = n.x * up.x + n.y * up.y + n.z * up.z;
  const dr = n.x * right.x + n.y * right.y + n.z * right.z;

  if (du === 1) return "SHAPE_ARROW_UP";
  if (du === -1) return "SHAPE_ARROW_DOWN";
  if (dr === 1) return "SHAPE_ARROW_RIGHT";
  if (dr === -1) return "SHAPE_ARROW_LEFT";

  throw new Error(`Unreachable arrow ${from} -> ${to}`);
}
