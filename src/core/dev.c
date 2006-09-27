#include "etherboot.h"
#include "stddef.h"
#include "dev.h"

/*
 * Each bus driver defines several methods, which are described in
 * dev.h.  This file provides a centralised, bus-independent mechanism
 * for locating devices and drivers.
 *
 */

/* Linker symbols for the various tables */
static struct bus_driver bus_drivers[0] __table_start ( bus_driver );
static struct bus_driver bus_drivers_end[0] __table_end ( bus_driver );
static struct device_driver device_drivers[0] __table_start ( device_driver );
static struct device_driver device_drivers_end[0] __table_end (device_driver );

/* Current attempted boot device */
struct dev dev = {
	.bus_driver = bus_drivers,
	.device_driver = device_drivers,
};

/*
 * Print all drivers 
 *
 */
void print_drivers ( void ) {
	struct device_driver *driver;

	for ( driver = device_drivers ;
	      driver < device_drivers_end ;
	      driver++ ) {
		printf ( "%s ", driver->name );
	}
}

/*
 * Move to the next location on any bus
 *
 */
static inline int next_location ( struct bus_driver **bus_driver,
				  struct bus_loc *bus_loc ) {
	/* Move to next location on this bus, if any */
	if ( (*bus_driver)->next_location ( bus_loc ) )
		return 1;

	/* Move to first (zeroed) location on next bus, if any */
	if ( ++(*bus_driver) < bus_drivers_end ) {
		DBG ( "DEV scanning %s bus\n", (*bus_driver)->name );
		return 1;
	}

	/* Reset to first bus, return "no more locations" */
	*bus_driver = bus_drivers;
	return 0;
}

/*
 * Find the next available device on any bus
 *
 * Set skip=1 to skip over the current device
 *
 */
int find_any ( struct bus_driver **bus_driver, struct bus_loc *bus_loc,
	       struct bus_dev *bus_dev, signed int skip ) {
	DBG ( "DEV scanning %s bus\n", (*bus_driver)->name );
	do {
		if ( --skip >= 0 )
			continue;
		if ( ! (*bus_driver)->fill_device ( bus_dev, bus_loc ) )
			continue;
		DBG ( "DEV found device %s\n",
		      (*bus_driver)->describe_device ( bus_dev ) );
		return 1;
	} while ( next_location ( bus_driver, bus_loc ) );

	DBG ( "DEV found no more devices\n" );
	return 0;
}

/*
 * Find a driver by specified device.
 *
 * Set skip=1 to skip over the current driver
 *
 */
int find_by_device ( struct device_driver **device_driver,
		     struct bus_driver *bus_driver, struct bus_dev *bus_dev,
		     signed int skip ) {
	do {
		if ( --skip >= 0 )
			continue;
		if ( (*device_driver)->bus_driver != bus_driver )
			continue;
		if ( ! bus_driver->check_driver ( bus_dev, *device_driver ))
			continue;
		DBG ( "DEV found driver %s for device %s\n",
		      (*device_driver)->name,
		      bus_driver->describe_device ( bus_dev ) );
		return 1;
	} while ( ++(*device_driver) < device_drivers_end );
	
	/* Reset to first driver, return "not found" */
	DBG ( "DEV found no driver for device %s\n",
	      bus_driver->describe_device ( bus_dev ) );
	*device_driver = device_drivers;
	return 0;
}

/*
 * Find a device by specified driver.
 *
 * Set skip=1 to skip over the current device
 *
 */
int find_by_driver ( struct bus_loc *bus_loc, struct bus_dev *bus_dev,
		     struct device_driver *device_driver,
		     signed int skip ) {
	struct bus_driver *bus_driver = device_driver->bus_driver;
	
	do {
		if ( --skip >= 0 )
			continue;
		if ( ! bus_driver->fill_device ( bus_dev, bus_loc ) )
			continue;
		if ( ! bus_driver->check_driver ( bus_dev, device_driver ) )
			continue;
		DBG ( "DEV found device %s for driver %s\n",
		      bus_driver->describe_device ( bus_dev ),
		      device_driver->name );
		return 1;
	} while ( bus_driver->next_location ( bus_loc ) );

	DBG ( "DEV found no device for driver %s\n", device_driver->name );
	return 0;
}

/*
 * Find the next available (device,driver) combination
 *
 * Set skip=1 to skip over the current (device,driver)
 *
 * Note that the struct dev may not have been previously used, and so
 * may not contain a valid (device,driver) combination.
 *
 */
int find_any_with_driver ( struct dev *dev, signed int skip ) {
	signed int skip_device = 0;
	signed int skip_driver = skip;

	while ( find_any ( &dev->bus_driver, &dev->bus_loc, &dev->bus_dev,
			   skip_device ) ) {
		if ( find_by_device ( &dev->device_driver, dev->bus_driver,
				      &dev->bus_dev, skip_driver ) ) {
			/* Set type_driver to be that of the device
			 * driver
			 */
			dev->type_driver = dev->device_driver->type_driver;
			/* Set type device instance to be the single
			 * instance provided by the type driver
			 */
			dev->type_dev = dev->type_driver->type_dev;
			return 1;
		}
		skip_driver = 0;
		skip_device = 1;
	}

	return 0;
}
