// NOTE: simple example
//        y = 0;
//        if (x == 0) {
//          y += 5;
//        }
//        
//        y += 5 * when_eq(x, 0)

vec4 when_eq(vec4 x, vec4 y) {
  return vec4(1.0) - abs(sign(x - y));
}

float when_eq(float x, float y)
{
    return 1.0f - abs(sign(x - y));
}

int when_eq(int x, int y)
{
    return 1 - abs(sign(x - y));
}

vec4 when_neq(vec4 x, vec4 y) {
  return abs(sign(x - y));
}

int when_neq(int x, int y)
{
    return abs(sign(x - y));
}

float when_gt(float x, float y)
{
    return max(sign(x - y), 0.0);
}

vec4 when_gt(vec4 x, vec4 y) {
  return max(sign(x - y), 0.0);
}

vec4 when_lt(vec4 x, vec4 y) {
  return max(sign(y - x), 0.0);
}

vec4 when_ge(vec4 x, vec4 y) {
  return vec4(1.0) - when_lt(x, y);
}

vec4 when_le(vec4 x, vec4 y) {
  return vec4(1.0) - when_gt(x, y);
}

vec4 and(vec4 a, vec4 b) {
  return a * b;
}

vec4 or(vec4 a, vec4 b) {
  return min(a + b, 1.0);
}

//vec4 xor(vec4 a, vec4 b) {
//  return (a + b) % 2.0;
//}

vec4 not(vec4 a) {
  return vec4(1.0) - a;
}