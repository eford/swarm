/*************
 *  Author : Saleh Dindar
 *
 *
 */
#include <iostream>
#include "swarm.h"
#include <boost/program_options.hpp>
#include <boost/program_options/positional_options.hpp>
#include "utils.hpp"

using namespace swarm;
using namespace std;

int DEBUG_LEVEL  = 0;
const int LOGARITHMIC_BASE = 2;

/* TODO: Move to appropriate location
   Note to self:  For Fermi GF100 (compute compat=2.0) and 
   assuming <=64 registers per thread
   14 SM * 8 blocks/SM * 64 threads/block = 7168 threads

   For hp_* integrators (or _cpt): Nbod<=7, threads/system = 3*Nbod, 
   so 14*8*21/Nbod = 2352/Nbod systems should be nearly optimal
   Nbod=3-> 784, Nbod=4-> 588, Nbod=5-> 470, Nbod=6-> 392, Nbod=7-> 336

   For thread/system integrators: threads/system = 1 
   registers per thread, 7168 systems should be nearly optimal
*/

void stability_test(config& cfg){
	if(!validate_configuration(cfg) ) {
		std::cerr << "Invalid configuration" << std::endl;
		return;
	}

	double duration = (cfg["duration"] != "") ? atof(cfg["duration"].c_str()) : 10 * M_PI ;        
	double interval = (cfg["interval"] != "") ? atof(cfg["interval"].c_str()) : (duration / 10 ) ; 
	double logarithmic = (cfg["logarithmic"] != "") ? atof(cfg["logarithmic"].c_str()) : 0 ; 

	if(interval < 1e-02 ) {
		std::cerr << "Interval is too small : " << interval << std::endl;
		return;
	}
	if(duration < interval ) {
		std::cerr << "Duration should be larger than interval : " << duration << std::endl;
		return;
	}

	// Initialize ensemble on host to be used with GPU integration.
	DEBUG_OUTPUT(1,"Generate initial conditions and save it into ensemble");
	cpu_ensemble reference_ensemble;
	generate_ensemble(cfg,reference_ensemble);

#if 1   // For testing if problems with hp_mvs are due to not being in COM frame
	// Shift into center-of-mass frame
	for(unsigned int i=0; i<reference_ensemble.nsys() ; ++i)
	  {
	    double cx=0., cy=0., cz=0., cvx=0., cvy=0., cvz=0., msum=0.;
	    for(unsigned int j=0; j<reference_ensemble.nbod(); ++j)
	      {
		msum += reference_ensemble.mass(i,j);
		cx   += reference_ensemble.mass(i,j) * reference_ensemble.x(i,j);
		cy   += reference_ensemble.mass(i,j) * reference_ensemble.y(i,j);
		cz   += reference_ensemble.mass(i,j) * reference_ensemble.z(i,j);
		cvx   += reference_ensemble.mass(i,j) * reference_ensemble.vx(i,j);
		cvy   += reference_ensemble.mass(i,j) * reference_ensemble.vy(i,j);
		cvz   += reference_ensemble.mass(i,j) * reference_ensemble.vz(i,j);
	      }
	    cx /= msum; cy /= msum; cz /= msum; cvx /= msum; cvy /= msum; cvz /= msum;
	    for(unsigned int j=0; j<reference_ensemble.nbod(); ++j)
	      {
		reference_ensemble.x(i,j) -= cx;
		reference_ensemble.y(i,j) -= cy;
		reference_ensemble.z(i,j) -= cz;
		reference_ensemble.vx(i,j) -= cvx;
		reference_ensemble.vy(i,j) -= cvy;
		reference_ensemble.vz(i,j) -= cvz;
	      }
	  }
#endif

	DEBUG_OUTPUT(3, "Make a copy of ensemble" );
	cpu_ensemble ens(reference_ensemble); // Make a copy of the CPU ensemble for comparison


	// performance stopwatches
	stopwatch swatch_kernel_gpu, swatch_upload_gpu, swatch_download_gpu, swatch_temps_gpu, swatch_init_gpu;
	stopwatch swatch_all;

	swatch_all.start(); // Start timer for entire program
	srand(42u);    // Seed random number generator, so output is reproducible



	// Start GPU timers for initialization
	swatch_init_gpu.start();

	std::auto_ptr<integrator> integ_gpu(integrator::create(cfg));
	cudaThreadSynchronize();  // Block until CUDA call completes
	swatch_init_gpu.stop();   // Stop timer for cpu initialization

	DEBUG_OUTPUT(1, "Upload ensemble to GPU" );
	cudaThreadSynchronize();   // Block until CUDA call completes
	swatch_upload_gpu.start(); // Start timer for copyg initial conditions to GPU
	gpu_ensemble gpu_ens(ens); // Initialize GPU ensemble, incl. copying data from CPU
	cudaThreadSynchronize();   // Block until CUDA call completes
	swatch_upload_gpu.stop();  // Stop timer for copyg initial conditions to GPU

	DEBUG_OUTPUT(1, "Set-up integrator data structors" );
	swatch_temps_gpu.start();  // Start timer for 0th step on GPU
	integ_gpu->set_default_log();
	integ_gpu->integrate(gpu_ens, 0.);  // a 0th step of dT=0 results in initialization of the integrator only
	cudaThreadSynchronize();   // Block until CUDA call completes
	swatch_temps_gpu.stop();   // Stop timer for 0th step on GPU

	std::cout << "# Time, Energy Conservation Error " << std::endl;

	
	for(double time = 0; time < duration ; ) {

		if((logarithmic > 1) && (time > 0)) interval = time * (logarithmic - 1);

		// TODO: change the way the step_size and tend are handeled
		double step_size = min(interval, duration - time );
		double stop_time = time + step_size;

		DEBUG_OUTPUT(1, "Integrator ensemble on GPU" );
		swatch_kernel_gpu.start(); // Start timer for GPU integration kernel
		integ_gpu->integrate(gpu_ens, stop_time);  // Actually do the integration w/ GPU!			
		cudaThreadSynchronize();  // Block until CUDA call completes
		swatch_kernel_gpu.stop(); // Stop timer for GPU integration kernel  

		DEBUG_OUTPUT(1, "Download data to host" );
		swatch_download_gpu.start();  // Start timer for downloading data from GPU
		ens.copy_from(gpu_ens);	// Download data from GPU to CPU		
		cudaThreadSynchronize();      // Block until CUDA call completes
		swatch_download_gpu.stop();   // Stop timer for downloading data from GPU

		DEBUG_OUTPUT(2, "Check energy conservation" );
		std::pair<double,double> max_med_deltaE = find_max_energy_conservation_error(ens, reference_ensemble );

				time += step_size;
				// time = ens.time(0);

				std::cout << time << ", " << max_med_deltaE.first << ", " << max_med_deltaE.second << ", " << ens.time(0) << std::endl;
				std::cout << std::flush;

	}

	/// CSV output for use in spreadsheet software 
	std::cerr << "\n# Benchmarking times \n" 
		<< "# Integration (ms), Integrator initialize (ms), "
		<< " Initialize (ms), GPU upload (ms), Download from GPU (ms) \n"  
		<< "# "
		<< swatch_kernel_gpu.getTime()*1000. << ",    "
		<< swatch_temps_gpu.getTime()*1000. << ", "
		<< swatch_init_gpu.getTime()*1000. << ", "
		<< swatch_upload_gpu.getTime()*1000. << ", "
		<< swatch_download_gpu.getTime()*1000. << ", "
		<< std::endl;

}


int main(int argc,  char **argv)
{
	namespace po = boost::program_options;


	// Parse command line arguements (making it easy to compare)
	po::positional_options_description pos;
	po::options_description desc(std::string("Usage: ") + argv[0] + " \nOptions");

	desc.add_options()
		("blocksize,b", po::value<std::string>() , "Threads per block")
		("duration,d", po::value<std::string>() , "Duration of the integration")
		("interval,i", po::value<std::string>() , "Logging interval")

		("logarithmic,l", po::value<std::string>() , "Produce times in logarithmic scale" )
		("num_sys,s", po::value<std::string>() , "number of systems")
		("timestep,t", po::value<std::string>() , "time step" )

		("cfg,c", po::value<std::string>(), "Integrator configuration file")
		("help,h", "produce help message")
		("verbose,v", po::value<int>(), "Verbosity level (debug output) ")
		;

	po::variables_map vm;
	po::store(po::command_line_parser(argc, argv).
			options(desc).positional(pos).run(), vm);
	po::notify(vm);

	// Print help message if requested or invalid parameters
	if (vm.count("help")) { std::cerr << desc << "\n"; return 1; }

	if (vm.count("verbose") ) DEBUG_LEVEL = vm["verbose"].as<int>();

	config cfg;

	// Default configuration
	{
		cfg["integrator"] = "hp_hermite"; // Set to use a GPU integrator
		cfg["runon"]      = "gpu";         // Set to runon GPU
		cfg["time step"] = "0.0005";       // time step
		cfg["precision"] = "1";
		cfg["duration"] = "31.41592";
		cfg["nbod"] = "3";
		cfg["nsys"] = "960";
		cfg["blocksize"] = "128";
	}

	if(vm.count("cfg")){
		std::string icfgfn =  vm["cfg"].as<std::string>();
		load_config(cfg,icfgfn);
	}

	if(vm.count("timestep")) {
		cfg["time step"] = vm["timestep"].as<std::string>();
	}

	if(vm.count("blocksize")) {
		cfg["blocksize"] = vm["blocksize"].as<std::string>();
	}

	if( ( cfg["blocksize"] != "" )  && (cfg["threads per block"] == "" )) 
		cfg["threads per block"] = cfg["blocksize"];

	if(vm.count("duration")) {
		cfg["duration"] = vm["duration"].as<std::string>();
	}
	if(vm.count("interval")) {
		cfg["interval"] = vm["interval"].as<std::string>();
	}
	if(vm.count("num_sys")) {
		cfg["nsys"] = vm["num_sys"].as<std::string>();
	}
	if(vm.count("logarithmic")) {
		cfg["logarithmic"] = vm["logarithmic"].as<std::string>();
	}

	std::cerr << "# Integrator:\t" << cfg["integrator"] << "\n"
		<< "# Time step\t" << cfg["time step"] << "\n"
		<< "# Interval\t" << cfg["interval"] << "\n"
		<< "# Min time step\t" << cfg["min time step"] << "\n"
		<< "# Max time step\t" << cfg["max time step"] << "\n"
		<< "# No. Systems\t" << cfg["nsys"] << "\n"
		<< "# No. Bodies\t" << cfg["nbod"] << "\n"
		  << "# Blocksize\t" << cfg["blocksize"] // << "\n"
		<< std::endl;

	////////////// STABILITY TEST /////// 
	DEBUG_OUTPUT(1,"Initialize swarm library ");
	swarm::init(cfg);         // Initialize the library

	stability_test(cfg);


}
