typedef struct Vector3 {
    float x, y, z;
} Vector3;

Vector3 vadd(const Vector3 v1, const Vector3 v2) {
    return (Vector3){
        .x = v1.x + v2.x,
        .y = v1.y + v2.y,
        .z = v1.z + v2.z,
    };
}
