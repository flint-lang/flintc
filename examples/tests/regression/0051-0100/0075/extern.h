typedef struct Vec3 {
    float values[3];
} Vec3;

void add(Vec3 *lhs, const Vec3 rhs) {
    lhs->values[0] += rhs.values[0];
    lhs->values[1] += rhs.values[1];
    lhs->values[2] += rhs.values[2];
}
