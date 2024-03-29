#version 460

#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_ray_query : require

// 128 invocations (threads) in parallel
layout(local_size_x = 16, local_size_y = 8, local_size_z = 1) in;

layout(set = 0, binding = 0, scalar) buffer storageBuffer
{
  vec3 imageData[];
};

layout(set = 0, binding = 1) uniform accelerationStructureEXT TLAS;

layout(set = 0, binding = 2, scalar) buffer vertexBuffer
{
    vec3 vertices[];
};

layout(set = 0, binding = 3, scalar) buffer indexBuffer
{
    uint indices[];
};

vec3 GetSkyColor(vec3 direction)
{
    if(direction.y <= 0.0f) return vec3(0.03f);

    return mix(vec3(1.0f), vec3(0.25f,0.5f,1.0f),direction.y);
}

struct HitResult
{
    vec3 color;
    vec3 worldPos;
    vec3 worldNormal;
};

HitResult GetObjectHitResult(rayQueryEXT rq)
{
  HitResult hr;
  // Get the ID of the triangle
  const int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rq, true);
    
  // Get the indices of the vertices of the triangle
  const uint i0 = indices[3 * primitiveID + 0];
  const uint i1 = indices[3 * primitiveID + 1];
  const uint i2 = indices[3 * primitiveID + 2];

  // Get the vertices of the triangle
  const vec3 v0 = vertices[i0];
  const vec3 v1 = vertices[i1];
  const vec3 v2 = vertices[i2];

  // Get the barycentric coordinates of the intersection
  vec3 barycentrics = vec3(0.0, rayQueryGetIntersectionBarycentricsEXT(rq, true));
  barycentrics.x    = 1.0 - barycentrics.y - barycentrics.z;

  // Compute the coordinates of the intersection
  const vec3 objectPos = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
  // For the main tutorial, object space is the same as world space:
  hr.worldPos = objectPos;
  
  const vec3 objectNormal = normalize(cross(v1 - v0, v2 - v0));
  // For the main tutorial, object space is the same as world space:
  hr.worldNormal = objectNormal;
  hr.color = vec3(0.5) + 0.5 * hr.worldNormal;

  /*
  const float dotX = dot(hr.worldNormal, vec3(1,0,0));
  if(dotX > 0.99)
  {
     hr.color = vec3(0.8,0.0,0);
  }
  else if(dotX < -0.99)
  {
   hr.color = vec3(0,0.8,0);
  }
  */
  // vec3(0.7f);

  return hr;
}

// RNG using pcg32i_random_t, using inc = 1. Our random state is a uint.
uint stepRNG(uint rngState) { return rngState * 747796405 + 1; }

// NOTE: inout in glsl works like reference in C++
// Steps the RNG and returns a floating-point value in range [0, 1].
float stepAndOutputRNGFloat(inout uint rngState)
{
  // Condensed version of pcg_output_rxs_m_xs_32_32, with simple conversion to floating-point [0, 1].
  rngState  = stepRNG(rngState);
  uint word = ((rngState >> ((rngState >> 28) + 4)) ^ rngState) * 277803737;
  word      = (word >> 22) ^ word;
  return float(word) / 4294967295.0f;
}

#define DENOISE 1
#define GLOSSY_MAT 0

void main()
{
  const uvec2 resolution = uvec2(800, 600);
  const uvec2 pixel = gl_GlobalInvocationID.xy;

  if((pixel.x >= resolution.x) || (pixel.y >= resolution.y)) return;
  // State of the random number generator.
  uint rngState = resolution.x * pixel.y + pixel.x;  // Initial seed

  // This scene uses a right-handed coordinate system like the OBJ file format, where the
  // +x axis points right, the +y axis points up, and the -z axis points into the screen.
  const vec3 cameraPos = vec3(-0.0001, 1, 6);

  // Next, define the field of view by the vertical slope of the topmost rays,
  // and create a ray direction:
  const float fovVerticalSlope = 1.0 / 5.0;

  // The sum of the colors of all of the samples.
  vec3 summedPixelColor = vec3(0.0);

  #if DENOISE
  const int samplesNum = 64;
  for(int sampleIdx = 0; sampleIdx < samplesNum; ++sampleIdx)
  {
  #endif
      vec3 rayOrigin = cameraPos;

      // Compute the direction of the ray for this pixel. To do this, we first
      // transform the screen coordinates to look like this, where a is the
      // aspect ratio (width/height) of the screen:
      //           1
      //    .------+------.
      //    |      |      |
      // -a + ---- 0 ---- + a
      //    |      |      |
      //    '------+------'
      //          -1
      
      #if DENOISE
      const vec2 randomPixelCenter = vec2(pixel) + vec2(stepAndOutputRNGFloat(rngState), stepAndOutputRNGFloat(rngState));
      const vec2 screenUV = vec2(2.0 * (float(randomPixelCenter.x) + 0.5 - 0.5 * resolution.x) / resolution.y,    //
                                 -(2.0 * (float(randomPixelCenter.y) + 0.5 - 0.5 * resolution.y) / resolution.y)  // Flip the y axis
      );
      #else
        const vec2 screenUV = vec2(2.0 * (float(pixel.x) + 0.5 - 0.5 * resolution.x) / resolution.y,    //
                                 -(2.0 * (float(pixel.y) + 0.5 - 0.5 * resolution.y) / resolution.y)  // Flip the y axis
      );
      #endif

      vec3 rayDirection     = normalize(vec3(fovVerticalSlope * screenUV.x, fovVerticalSlope * screenUV.y, -1.0));

      vec3 accumulatedRayColor = vec3(1.0);  // The amount of light that made it to the end of the current ray.
      vec3 pixelColor          = vec3(0.0);

      const uint bounceCount=32;
      // Limit the kernel to trace at most 32 segments. (up to 31 bounces)
      for(int bounceIdx = 0; bounceIdx < bounceCount; bounceIdx++)
      {
        // Trace the ray and see if and where it intersects the scene!
        // First, initialize a ray query object:
        rayQueryEXT rayQuery;
        rayQueryInitializeEXT(rayQuery,              // Ray query
                              TLAS,                  // Top-level acceleration structure
                              gl_RayFlagsOpaqueEXT,  // Ray flags, here saying "treat all geometry as opaque"
                              0xFF,                  // 8-bit instance mask, here saying "trace against all instances"
                              rayOrigin,             // Ray origin
                              0.0,                   // Minimum t-value
                              rayDirection,          // Ray direction
                              10000.0);              // Maximum t-value

        // Start traversal, and loop over all ray-scene intersections. When this finishes,
        // rayQuery stores a "committed" intersection, the closest intersection (if any).
        while(rayQueryProceedEXT(rayQuery))
        {
        }

        // Get the type of committed (true) intersection - nothing, a triangle, or
        // a generated object
        if(rayQueryGetIntersectionTypeEXT(rayQuery, true) != gl_RayQueryCommittedIntersectionTriangleEXT) 
        {
        #if DENOISE
           accumulatedRayColor *= GetSkyColor(rayDirection);

           // Sum this with the pixel's other samples.
           // (Note that we treat a ray that didn't find a light source as if it had
           // an accumulated color of (0, 0, 0)).
           summedPixelColor += accumulatedRayColor;
        #else 
           pixelColor = accumulatedRayColor * GetSkyColor(rayDirection);
        #endif
           break;
        }
    
        // Ray hit a triangle
        HitResult hr = GetObjectHitResult(rayQuery);

        // Apply color absorption
        accumulatedRayColor *= hr.color;

        // Flip the normal so it points against the ray direction based on triangle face
        // So, faceforward(N, I, Nref) returns N, if dot(I, Nref) < 0, and -N otherwise.
        hr.worldNormal = faceforward(hr.worldNormal, rayDirection, hr.worldNormal);

        // Start a new ray at the hit position:
        // HINT: To avoid self-intersection we adding some offset
        rayOrigin = hr.worldPos + 0.0001 * hr.worldNormal;

        // For a random diffuse bounce direction, we follow the approach of
        // Ray Tracing in One Weekend, and generate a random point on a sphere
        // of radius 1 centered at the normal. This uses the random_unit_vector
        // function from chapter 8.5:
        const float theta = 6.2831853 * stepAndOutputRNGFloat(rngState);   // Random in [0, 2pi]
        const float u     = 2.0 * stepAndOutputRNGFloat(rngState) - 1.0;  // Random in [-1, 1]
        const float r     = sqrt(1.0 - u * u);
        #if GLOSSY_MAT
           rayDirection      = normalize(reflect(rayDirection, hr.worldNormal) + 0.2 * vec3(r * cos(theta), r * sin(theta), u));
        #else
           rayDirection      = normalize(hr.worldNormal + vec3(r * cos(theta), r * sin(theta), u));
        #endif

        // If our material is glossy
        // Reflect the direction of the ray using the triangle normal:
        // rayDirection = reflect(rayDirection, hr.worldNormal);
      }
 #if DENOISE
  }
  #endif

  const uint linearIndex = resolution.x * pixel.y + pixel.x;
  #if DENOISE
  imageData[linearIndex] = summedPixelColor / float(samplesNum);
  #else
  imageData[linearIndex] = pixelColor;
  #endif


  /*
  // Start traversal, and loop over all ray-scene intersections. When this finishes,
  // rayQuery stores a "committed" intersection, the closest intersection (if any).
  while(rayQueryProceedEXT(rayQuery))
  {
  }

  // Get the t-value of the intersection (if there's no intersection, this will
  // be tMax = 10000.0). "true" says "get the committed intersection."
   //const float t = rayQueryGetIntersectionTEXT(rayQuery, true);

   vec3 pixelColor;
  // Get the type of committed (true) intersection - nothing, a triangle, or
  // a generated object
  if(rayQueryGetIntersectionTypeEXT(rayQuery, true) == gl_RayQueryCommittedIntersectionTriangleEXT)
  {
    // BARYCENTRICS
    //{
    // Create a vec3(0, b.x, b.y)
    //pixelColor = vec3(0.0, rayQueryGetIntersectionBarycentricsEXT(rayQuery, true));
    // Set the first element to 1 - b.x - b.y, setting pixelColor to
    // (1 - b.x - b.y, b.x, b.y).
    //pixelColor.x = 1 - pixelColor.y - pixelColor.z;
    //}

    // PRIMITIVES
   // const int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);
   // pixelColor            = vec3(primitiveID / 10.0, primitiveID / 100.0, primitiveID / 1000.0);

   // FIRST VERTEX COLOR
   
   // const int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);

//    const uint i0 = indices[3 * primitiveID+0];
  //  const vec3 v0 = vertices[i0];
    //pixelColor = vec3(0.5) + 0.25 * v0;
    

    const int primitiveID = rayQueryGetIntersectionPrimitiveIndexEXT(rayQuery, true);

      // Get the indices of the vertices of the triangle
    const uint i0 = indices[3 * primitiveID + 0];
    const uint i1 = indices[3 * primitiveID + 1];
    const uint i2 = indices[3 * primitiveID + 2];

    // Get the vertices of the triangle
    const vec3 v0 = vertices[i0];
    const vec3 v1 = vertices[i1];
    const vec3 v2 = vertices[i2];

    const vec3 objectNormal = normalize(cross(v1 - v0, v2 - v0));
    pixelColor = vec3(0.5) + 0.5 * objectNormal;
  }
  else
  {
    // Ray hit the sky
    pixelColor = vec3(0.0, 0.0, 0.5);
  }

  const uint linearIndex       = resolution.x * pixel.y + pixel.x;
  imageData[linearIndex] = pixelColor;// vec3(t / 10.0);
  */
  /*
  const vec3 pixelColor = vec3(float(pixel.x) / resolution.x, float(pixel.y) / resolution.y,0.0);                 
  const uint linearIndex = resolution.x * pixel.y + pixel.x;

  imageData[linearIndex] = pixelColor;
  */
}