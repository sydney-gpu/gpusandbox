//////////////////////////////////////////////////////////////////////////
// Author:	Conan Bourke & Tomasz Bednarz
// Date:	June 4 2012
//
// Copyright (c) 2012
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
// 
//////////////////////////////////////////////////////////////////////////
typedef struct Params
{
	float neighbourRadiusSqr;

	float maxSteeringForce;
	float maxBoidSpeed;

	float wanderRadius;
	float wanderJitter;
	float wanderDistance;
	float wanderWeight;

	float separationWeight;
	float cohesionWeight;
	float alignmentWeight;

	unsigned int boidCount;
} Params;

float4 truncate(float maxSqr, float4 num)
{
	float magSqr = num.x * num.x + num.y * num.y + num.z * num.z;
	
	if (magSqr > maxSqr)
	{
		float scale = maxSqr / magSqr;
		num.x *= scale;
		num.y *= scale;
		num.z *= scale;
	}
	
	return num;
}

float lengthSqr(float4 n)
{
	return n.x * n.x + n.y * n.y + n.z * n.z;
}

// simple pseudo-random number generation
float rand(float2 co)
{
	float junk = 0;
	return fract(sin(dot(co, (float2)(12.9898f, 78.233f))) * 43758.5453f, &junk);
}

kernel void flocking(
		global float4* vPosition,
		global float4* vVelocity,
		global float4* vWanderTarget,
		constant struct Params* pp,
		float deltaTime
	)
{
	unsigned int i = get_global_id(0);

	unsigned int j, uiNeighbourCount;

	float4 vSteeringForce = (float4)0.0f;
	float4 vSeparation = (float4)0.0f;
	float4 vCohesion = (float4)0.0f;
	float4 vAlignment = (float4)0.0f;
	float4 vWander = (float4)0.0f;

	float3 vHeading = fast_normalize(vVelocity[i].xyz);

	for (j = 0, uiNeighbourCount = 0 ; j < pp->boidCount; ++j)
	{
		if (i == j) continue;

		float4 vTo = vPosition[i] - vPosition[j];
		vTo.w = 0;
		float fDistSqr = vTo.x * vTo.x + vTo.y * vTo.y + vTo.z * vTo.z;

		if (fDistSqr < pp->neighbourRadiusSqr)
		{
			uiNeighbourCount += 1;

			// sum separation
			if (fDistSqr != 0)
			{
				vTo = fast_normalize(vTo);
				vSeparation += vTo / sqrt(fDistSqr);
			}

			// sum averages
			vCohesion += vPosition[j];
			vAlignment.xyz += fast_normalize( vVelocity[j].xyz );
		}
	}

	vVelocity[i].w = (float)uiNeighbourCount;

	// apply cohesion and alignment
	if (uiNeighbourCount > 0)
	{
		// cohesion
		vCohesion /= vVelocity[i].w;
 		if (vPosition[i].x != vCohesion.x || 
			vPosition[i].y != vCohesion.y || 
			vPosition[i].z != vCohesion.z)
		{
			vCohesion = fast_normalize(vCohesion - vPosition[i]) * pp->maxBoidSpeed - (float4)(vVelocity[i].xyz, 0.0f);
		}

		// alignment
		vAlignment /= vVelocity[i].w;
		vAlignment.xyz -= vHeading;
	}	

	// wander	
	vWanderTarget[i].x += rand(vPosition[i].xy * 42)*pp->wanderJitter;
	vWanderTarget[i].y += rand(vPosition[i].xz * 666)*pp->wanderJitter;
	vWanderTarget[i].z += rand(vPosition[i].yz * 42)*pp->wanderJitter;
	vWanderTarget[i] = fast_normalize(vWanderTarget[i]) * pp->wanderRadius;
	vWander = (vWanderTarget[i] + (float4)(vHeading,0.0f) * pp->wanderDistance) - vPosition[i];

	float maxForceSqr = pp->maxSteeringForce * pp->maxSteeringForce;

	// sum forces (prioritised)
	// wander -> separation -> cohesions -> alignment
	vSteeringForce += vWander * pp->wanderDistance * pp->wanderWeight;
	vSteeringForce = truncate(maxForceSqr, vSteeringForce);
	
	if (lengthSqr(vSteeringForce) < maxForceSqr)
	{
		vSteeringForce += vSeparation * pp->separationWeight;
		vSteeringForce = truncate(maxForceSqr, vSteeringForce);

		if (lengthSqr(vSteeringForce) < maxForceSqr)
		{
			vSteeringForce += vCohesion * pp->cohesionWeight;
			vSteeringForce = truncate(maxForceSqr, vSteeringForce);

			if (lengthSqr(vSteeringForce) < maxForceSqr)
			{
				vSteeringForce += vAlignment * pp->alignmentWeight;
				vSteeringForce = truncate(maxForceSqr, vSteeringForce);
			}
		}
	}
	
	// apply force to velocity
	vVelocity[i].xyz += vSteeringForce.xyz * deltaTime;
	vVelocity[i] = truncate(pp->maxBoidSpeed * pp->maxBoidSpeed, vVelocity[i]);

	// barrier causes threads to halt until all threads have reached this point
	barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);

	vPosition[i].xyz += vVelocity[i].xyz * deltaTime;

	if (vPosition[i].x > 100)
		vPosition[i].x = -100;
	if (vPosition[i].x < -100)
		vPosition[i].x = 100;

	if (vPosition[i].y > 100)
		vPosition[i].y = -100;
	if (vPosition[i].y < -100)
		vPosition[i].y = 100;

	if (vPosition[i].z > 100)
		vPosition[i].z = -100;
	if (vPosition[i].z < -100)
		vPosition[i].z = 100;

	barrier(CLK_LOCAL_MEM_FENCE | CLK_GLOBAL_MEM_FENCE);
}

