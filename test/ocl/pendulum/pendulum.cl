#ifdef INSIEME 
#include "ocl_device.h" 
#endif 

/*#ifdef cl_amd_fp64
#pragma OPENCL EXTENSION cl_amd_fp64 : enable
#endif*/

#ifdef cl_khr_fp64
#pragma OPENCL EXTENSION cl_khr_fp64 : enable
#endif

#define sqr(x) (x)*(x)
#define ABS(x) sqrt( sqr(x[0]) + sqr(x[1]) )
#define DIST(x, y) sqrt(sqr((x)[0]-(y)[0])+sqr((x)[1]-(y)[1]))
#define ASS(x, y) ((x)[0] = (y)[0], (x)[1] = (y)[1])

enum Kind { Linear, Magnet };

typedef struct {
	enum Kind type;
	double pos[2];
	double mult;
	double size;
} Source;

typedef struct {
	unsigned target;
	unsigned numSteps;
} Trace;


Trace getTarget(double i, double j,
	__global Source* sources, unsigned num_sources,
	double dt, double friction, double height,
	unsigned min_steps, unsigned max_steps,
	double abortVelocity) {
        
	// pendulum properties pendulum
	double pos[2] = {i,j};  // the current position
	double vel[2] = {0,0};  // the velocity

	// acceleration
	double acc[2] = {0,0};
	double acc_new[2] = {0,0};
	double acc_old[2] = {0,0};

	// some loop-invariant "constants"
	double sqrt_dt = sqrt(dt);

	// iterate until close to a source and velocity is small
	for (unsigned step = 0; step < max_steps; ++step) {

		// ----- update position -----
		pos[0] += vel[0] * dt + sqrt_dt * (2.0/3.0 * acc[0] - 1.0/6.0 * acc_old[0]);
		pos[1] += vel[1] * dt + sqrt_dt * (2.0/3.0 * acc[1] - 1.0/6.0 * acc_old[1]);

		acc_new[0] = 0;
		acc_new[1] = 0;

		// 1) calculate forces (proportional to the inverse square of the distance)
		for (unsigned i = 0; i < num_sources; i++) {
			__global Source* cur = &(sources[i]);
			unsigned k =i;

			double r[2] = { pos[0] - cur->pos[0], pos[1] - cur->pos[1] };

			if (cur->type == Linear) {
				// Hooke's law (pulling back the pendulum to (0,0)

				// add effect to forces
				acc_new[0] -= cur->mult * r[0];
				acc_new[1] -= cur->mult * r[1];

			} else {
				// Magnet forces: m * a = k * abs(r) / abs(r^3)

				double dist = sqrt ( sqr(pos[0] - cur->pos[0]) + sqr(pos[1] - cur->pos[1]) + sqr(height) );

				// add effect to forces
				acc_new[0] -= (cur->mult / (dist*dist*dist)) * r[0];
				acc_new[1] -= (cur->mult / (dist*dist*dist)) * r[1];
			}

			// check whether close enough to current source
			if (step > min_steps) if(  ABS(r) < cur->size) if( ABS(vel) < abortVelocity) {

				// this is the right magnet
				return (Trace){i, step};
			}
		}

		// 2) add (linear) friction proportinal to velocity
		acc_new[0] -= vel[0] * friction;
		acc_new[1] -= vel[1] * friction;

		// 3) update velocity
		vel[0] += dt * ( 1.0/3.0 * acc_new[0] + 5.0/6.0 * acc[0] - 1.0/6.0 * acc_old[0]);
		vel[1] += dt * ( 1.0/3.0 * acc_new[1] + 5.0/6.0 * acc[1] - 1.0/6.0 * acc_old[1]);

		// 4) flip the accelerator values
		ASS(acc_old, acc);
		ASS(acc, acc_new);
	}

	// undecided after MAX number of steps => return num_sources + 1
	return (Trace){num_sources, max_steps};
}

#pragma insieme mark
__kernel void pendulum(	__global unsigned* buf_image, 
			__global unsigned* buf_dist,
			__global Source* buf_sources,
			unsigned num_sources,
			double dt,
			double friction,
			double height,
			unsigned min_steps,
			unsigned max_steps,
			double abortVelocity,
			int width,
			int num_elements,
			double scale
			) {

	int gid = get_global_id(0);
	if (gid >= num_elements) return;

        int tx = gid / width;
        int ty = gid % width;
        double curX = (-1.0 + (double)tx/(double)(width-1) * 2.0) * scale;
        double curY = (-1.0 + (double)ty/(double)(width-1) * 2.0) * scale;

        Trace res = getTarget(curX, curY, buf_sources, num_sources, dt, friction, height, min_steps, max_steps, abortVelocity);

        buf_image[gid] = res.target;
        buf_dist [gid] = res.numSteps;
}
