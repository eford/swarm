#include "swarm/swarm.h"
#include "swarm/snapshot.hpp"
#include <boost/python.hpp>
#include <boost/python/suite/indexing/map_indexing_suite.hpp>

using std::string;
using namespace boost::python;
using boost::noncopyable;
using namespace swarm;

gpu::Pintegrator create_gpu_integrator(const config& cfg){
	return boost::dynamic_pointer_cast<gpu::integrator>(integrator::create(cfg));
}

#define PROPERTY_ACCESSOR(CLASS,PROPERTY,TYPE)       \
	void set_##PROPERTY( CLASS &r, const TYPE & t)   \
	{ r.PROPERTY() = t;  	}                        \
	TYPE get_##PROPERTY( CLASS &r)                   \
	{ return r.PROPERTY(); }

PROPERTY_ACCESSOR(ensemble::SystemRef, time, double )
PROPERTY_ACCESSOR(ensemble::SystemRef, id  , int    )

PROPERTY_ACCESSOR(ensemble::Body     , mass, double )
PROPERTY_ACCESSOR(ensemble::Body::Component, pos, double )
PROPERTY_ACCESSOR(ensemble::Body::Component, vel, double )

config load_config(const std::string &fn){
	return config::load(fn);
}

void set_pos_list(ensemble::Body& b, const list& p ){
	b[0].pos() = extract<double>(p[0]);
	b[1].pos() = extract<double>(p[1]);
	b[2].pos() = extract<double>(p[2]);
}

list get_pos_list(const ensemble::Body& b){
	list p;
	p.append( b[0].pos() );
	p.append( b[1].pos() );
	p.append( b[2].pos() );
	return p;
}

void set_vel_list(ensemble::Body& b, const list& v ){
	b[0].vel() = extract<double>(v[0]);
	b[1].vel() = extract<double>(v[1]);
	b[2].vel() = extract<double>(v[2]);
}

list get_vel_list(const ensemble::Body& b){
	list v;
	v.append( b[0].vel() );
	v.append( b[1].vel() );
	v.append( b[2].vel() );
	return v;
}

/* Swarm as a python module
 *
 * The reason for lib prefix is because CMake automatically
 * adds lib prefix to the name of the target
 *
 */
BOOST_PYTHON_MODULE(libswarmng_ext) {

	def("init", swarm::init );
	def("generate_ensemble", generate_ensemble );
	def("sync", cudaThreadSynchronize );

	class_<config>("Config")
		.def( map_indexing_suite< config >() )
		.def("load", &load_config)
		.staticmethod("load")
		;

	class_<ensemble::Sys::attributes_t, noncopyable >( "Sys.Attributes", no_init )
		.def("__getitem__", &ensemble::Sys::attributes_t::getitem )
		.def("__setitem__", &ensemble::Sys::attributes_t::setitem )
		;

	class_<ensemble::Body::attributes_t, noncopyable >( "Body.Attributes", no_init )
		.def("__getitem__", &ensemble::Body::attributes_t::getitem )
		.def("__setitem__", &ensemble::Body::attributes_t::setitem )
		;

	class_<ensemble::Body::Component >("Body.Components", no_init)
		.add_property("pos", &get_pos, &set_pos )
		.add_property("vel", &get_vel, &set_vel )
		;

	/* All the essential properties are covered
	 * However, we can make some convenience access
	 * methods like x,y,z,vy,vx,vz
	 * or get positions and velocities as a python list
	 * or set the body from a python list, etc.
	 *
	 *
	 */
	class_<ensemble::Body >("Body", no_init )
		.add_property("mass", &get_mass, &set_mass)
		.def("radius", &ensemble::Body::radius )
		.def("speed", &ensemble::Body::speed )
		.def("attributes", &ensemble::Body::attributes, return_value_policy<reference_existing_object>() )
		.def("__getitem__", &ensemble::Body::getitem, return_value_policy<reference_existing_object>() )
		.add_property("pos", &get_pos_list, &set_pos_list ) 
		.add_property("vel", &get_vel_list, &set_vel_list ) 
		;

	/*
	 *  All important methods are covered for this class
	 */
	class_<ensemble::SystemRef  >("SystemRef", no_init )
		.add_property("time", &get_time, &set_time)
		.add_property("id", &get_id, &set_id)
		.add_property("total_energy", &ensemble::SystemRef::total_energy)
		.def("is_active", &ensemble::SystemRef::is_active )
		.def("is_inactive", &ensemble::SystemRef::is_inactive )
		.def("is_enabled", &ensemble::SystemRef::is_enabled )
		.def("set_active", &ensemble::SystemRef::set_active )
		.def("set_inactive", &ensemble::SystemRef::set_inactive )
		.def("set_disabled", &ensemble::SystemRef::set_disabled )
		.def("__getitem__", &ensemble::SystemRef::getitem, return_value_policy<reference_existing_object>() )
		.def("attributes", &ensemble::SystemRef::attributes, return_value_policy<reference_existing_object>() )

		;

	class_<ensemble, noncopyable >("Ensemble", no_init )
		.def("__length__", &ensemble::nsys , return_value_policy<copy_const_reference>() )
		.add_property("nsys", make_function(&ensemble::nsys , return_value_policy<copy_const_reference>() ) )
		.add_property("nbod", make_function(&ensemble::nbod , return_value_policy<copy_const_reference>() ) )
		.def("__getitem__", &ensemble::getitem )
		;

	class_<defaultEnsemble, bases<ensemble> >("DefaultEnsemble")
		.def("create", &defaultEnsemble::create )
		.staticmethod("create")
		.def( "clone", &defaultEnsemble::clone )
		.def("save_to_bin", &snapshot::save) 
		.def("save_to_text", &snapshot::save_text) 
		.def("load_from_bin", &snapshot::load) 
		.def("load_from_text", &snapshot::load_text) 
		.staticmethod("load_from_bin")
		.staticmethod("load_from_text")
		;
	

	class_<integrator, Pintegrator, noncopyable >("Integrator", no_init )
		.def("create",&integrator::create)
		.staticmethod("create")
		.def("integrate", &integrator::integrate )
		.add_property("ensemble", make_function(&integrator::get_ensemble, return_value_policy<reference_existing_object>() ), &integrator::set_ensemble)
		.add_property("destination_time", &integrator::get_destination_time, &integrator::set_destination_time)
		;

	void (gpu::integrator::*gpu_set_ensemble)(defaultEnsemble&) = &gpu::integrator::set_ensemble;

	class_<gpu::integrator, bases<integrator> , gpu::Pintegrator, noncopyable>("GpuIntegrator", no_init)
		.def("create", &create_gpu_integrator )
		.staticmethod("create")
		.def("integrate", &gpu::integrator::integrate )
		.def("core_integrate", &gpu::integrator::core_integrate )
		.def("download_ensemble", &gpu::integrator::download_ensemble )
		.def("upload_ensemble", &gpu::integrator::upload_ensemble )
		.add_property("ensemble", make_function(&integrator::get_ensemble, return_value_policy<reference_existing_object>() ), gpu_set_ensemble)
		;

	def("find_max_energy_conservation_error", find_max_energy_conservation_error );





}
