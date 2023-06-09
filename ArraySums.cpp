#include <iostream>
#include <getopt.h>
#include <cstdlib>
#include <iomanip>
#include <cassert>
#include <arm_neon.h>
#include <vector>
#include <thread>
#include <sys/time.h>
#include <cstring>

using namespace std;
void SingleCore(float * a, float * b, float * c, int size);
double SumOfSums(float * p, int size);
bool HandleOptions(int argc, char ** argv, int & an_iter, int & a_size);
void fillArrays(float * & a, float * & b, int size);
void clear(float* a, float* b, float* c);
void Intrinsic(float * a, float * b, float * c, int size);

int main(int argc, char * argv[])
{
	//set up
	int an_iter = 1;
	int a_size = 128;
	bool check = HandleOptions(argc, argv, an_iter, a_size);
	if(!check)
	{
		//if here handle options returned false
		//if the error is unspecified beforehand it is because the number entered was equal to or greater than 0
		cerr << "Program ran into an error, exiting with 1\n";
		return 1;
	}
	int remainder = a_size % 16;
	if (remainder > 0)
	{
		//if here adjusting input size to a number divisible by 16
		int a_size = a_size + 16 - remainder;
	}
	//spawn the float pointers with alligned allocation
	float * a = (float *) aligned_alloc(16, a_size * sizeof(float));
	float * b = (float *) aligned_alloc(16, a_size * sizeof(float));
	float * c = (float *) aligned_alloc(16, a_size * sizeof(float));
	
	//single core naive
	struct timeval begin, end;
	srand(time(NULL));
	fillArrays(a, b, a_size);
	gettimeofday(&begin, 0);
	for (int i = 0; i < an_iter; i++)
	{
		//iterates throught the single core
		SingleCore(a, b, c, a_size);
	}
	gettimeofday(&end, 0);
	size_t cSize = sizeof(c);
    long seconds = end.tv_sec - begin.tv_sec;
    long microseconds = end.tv_usec - begin.tv_usec;
    double elapsed = seconds + microseconds*1e-6;
	double elapsed_avg1 = elapsed / an_iter;
	double total1 = SumOfSums(c, a_size);
    
    
	//single core neon
	memset(c, 0, cSize);
	gettimeofday(&begin, 0);
	for (int i = 0; i < an_iter; i++)
	{
		Intrinsic(a, b, c, a_size);
	}
	gettimeofday(&end, 0);
	cSize = sizeof(c);
    seconds = end.tv_sec - begin.tv_sec;
    microseconds = end.tv_usec - begin.tv_usec;
    elapsed = seconds + microseconds*1e-6;
	double elapsed_avg2 = elapsed / an_iter;
	double total2 = SumOfSums(c, a_size);

	//multicore naive
	memset(c, 0, cSize);
	int cores = std::thread::hardware_concurrency(); //number of cores
	vector <std::thread> the_threads;
	float * ctemp = c;
	float * atemp = a;
	float * btemp = b;
	int part_size = a_size / 16;
	float frem = part_size % cores;
	frem = frem / cores;
	part_size = (part_size / cores) - frem; //holds the portion size for each thread when traversing c array
	gettimeofday(&begin, 0);
	for (int i = 0; i < an_iter; i++)
	{
		for (int i = 0; i < cores - 1; i++)
		{
			//spawns the threads calling multiintrinsic
			the_threads.push_back(std::thread(SingleCore, atemp, btemp, ctemp, part_size));
			ctemp += part_size;
			atemp += part_size;
			btemp += part_size;
		}
		ctemp += (a_size/16) % cores;
		atemp += (a_size/16) % cores;
		btemp += (a_size/16) % cores;
		part_size += (a_size/16) % cores;
		the_threads.push_back(std::thread(SingleCore, atemp, btemp, ctemp, part_size)); //accounts for edge case where size / 16 is not divisible by cores
		for (int i = 0; i < cores; i++)
		{
			//joins the threads
			the_threads[i].join();
		}
		the_threads.clear();
	}
	gettimeofday(&end, 0);
	cSize = sizeof(c);
    seconds = end.tv_sec - begin.tv_sec;
    microseconds = end.tv_usec - begin.tv_usec;
    elapsed = seconds + microseconds*1e-6;
	double elapsed_avg3 = elapsed / an_iter;
	double total3 = SumOfSums(c, a_size);
	//multicore intrinsic
	memset(c, 0, cSize);
	ctemp = c;
	atemp = a;
	btemp = b;
	the_threads.clear();
	part_size -= (a_size/16) % cores; //holds the portion size for each thread when traversing c array
	gettimeofday(&begin, 0);
	for (int i = 0; i < an_iter; i++)
	{
		for (int i = 0; i < cores - 1; i++)
		{
			//spawns the threads calling multiintrinsic
			the_threads.push_back(std::thread(Intrinsic, atemp, btemp, ctemp, part_size));
			ctemp += part_size;
			atemp += part_size;
			btemp += part_size;
		}
		ctemp += (a_size/16) % cores;
		atemp += (a_size/16) % cores;
		btemp += (a_size/16) % cores;
		part_size += (a_size/16) % cores;
		the_threads.push_back(std::thread(Intrinsic, atemp, btemp, ctemp, part_size));
		for (int i = 0; i < cores; i++)
		{
			//joins the threads
			the_threads[i].join();
		}
		the_threads.clear();
	}	
	gettimeofday(&end, 0);
	cSize = sizeof(c); //gets the size of c array in bytes
    seconds = end.tv_sec - begin.tv_sec;
    microseconds = end.tv_usec - begin.tv_usec;
    elapsed = seconds + microseconds*1e-6;
	double elapsed_avg4 = elapsed / an_iter;
	double total4 = SumOfSums(c, a_size);
	printf("Array size: %i total size in MB: %zu\nIterations: %i\nAvailable cores: %i\nNaive: %.6f Check: %.6f\nNeon: %.6f Check: %.6f\nThreading...\nNaive: %.6f Check: %.6f\nNeon: %.6f Check: %.6f\n", a_size, cSize, an_iter, cores, elapsed_avg1, total1, elapsed_avg2, total2, elapsed_avg3, total3, elapsed_avg4, total4);
	
	clear(a, b, c); //frees the arrays

    return 0;
}

void clear(float* a, float* b, float* c) //clears the arrays
{
	free(a);
	free(b);
	free(c);
}
void SingleCore(float * a, float * b, float * c, int size) //adds a and b to populate c using a single core
{
	assert((size & 0xF) == 0);
	size = size / 16;
	for (int i = 0; i < size; i++) 
	{
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
		*(c++) = *(a++) + *(b++);
	}
	
}

bool HandleOptions(int argc, char ** argv, int & an_iter, int & a_size) //handles the command line arguments
{
	int c;
	while ((c = getopt(argc, argv, "hi:s:")) != -1) {
		switch (c) {
			default:
			case 'h':
				cout << "help:\n";
				cerr << "-h       prints this\n";
				cerr << "-s size  sets size (default is 128 - will be made divisible by 16)\n";
				cerr << "-i iter  sets iterations (default is 1)\n";
				return false;
			case 'i':
				an_iter = atoi(optarg);
				break;
			case 's':
				a_size = atoi(optarg);
				break;
			case '?':
				cerr << "Unhandled command line option\n";
				return false;
		}
	}
	return a_size > 0;
}
void fillArrays(float * & a, float * & b, int size) //fills a and b array with numbers
{
	float num;
	float * at = a;
	float * bt = b;
	for (int i = 0; i < size; i++)
	{
		num = static_cast<float>(rand()) * static_cast<float>(1) / RAND_MAX;
		*(a++) = num;
		num = static_cast<float>(rand()) * static_cast<float>(1) / RAND_MAX;
		*(b++) = num;
	}
	a = at;
	b = bt;
}
double SumOfSums(float * p, int size) //takes the sum of all the values in c
{
	double sum;
	for (int i = 0; i < size; i++)
	{
		sum += *(p++);
	}
	return sum;
}

void Intrinsic(float * a, float * b, float * c, int size) //adds a and b to populate c using single core and intrinsics
{
	assert((size & 0xF) == 0);
	size = size / 16;
	float32x4_t an; 
	float32x4_t bn; 
	for (int i = 0; i < size; i++) 
	{
		vst1q_f32(c, vaddq_f32(vld1q_f32(a), vld1q_f32(b)));
		a += 4;
		b += 4;
		c += 4;
		vst1q_f32(c, vaddq_f32(vld1q_f32(a), vld1q_f32(b)));
		a += 4;
		b += 4;
		c += 4;
		vst1q_f32(c, vaddq_f32(vld1q_f32(a), vld1q_f32(b)));
		a += 4;
		b += 4;
		c += 4;
		vst1q_f32(c, vaddq_f32(vld1q_f32(a), vld1q_f32(b)));
		a += 4;
		b += 4;
		c += 4;
	}
}
