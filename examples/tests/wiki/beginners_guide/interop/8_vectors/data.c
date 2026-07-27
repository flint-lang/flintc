typedef struct {
    float x, y, z;
} Vector3;

Vector3 add(const Vector3 v1, const Vector3 v2) {
    Vector3 result = v1;
    result.x += v2.x;
    result.y += v2.y;
    result.z += v2.z;
    return result;
}
