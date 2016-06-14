vision Qserv using Docker+Openstack
===================================

Pre-requisite
-------------

* Install system dependencies

.. code-block:: bash

   sudo apt-get install python-dev python-pip

   # Install the OpenStack client
   sudo pip install python-openstackclient

   # Install the nova client
   sudo pip install python-novaclient

* Download Openstack RC file: http://docs.openstack.org/user-guide/common/cli_set_environment_variables_using_openstack_rc.html

* Install shmux to run multi node tests
see http://web.taranis.org/shmux/


Provision Qserv & run multinode tests
-------------------------------------
   
* Clone Qserv repository and set Openstack environment and parameters:

.. code-block:: bash

   SRC_DIR=${HOME}/src
   mkdir ${SRC_DIR}
   cd ${SRC_DIR}
   git clone https://github.com/lsst/qserv.git
   cd qserv/admin/tools/provision

   # Source Openstack RC file
   # This is an example for NCSA 
   . ./LSST-openrc.sh

   # Update the configuration file which contains instance parameters
   # Add special tuning if needed
   mv ncsa.conf.example ncsa.conf

Update :program:`test.sh` top parameters to set the configuration files above, the snapshot name and the number of servers to boot.

* Create customized image, provision openstack cluster and run Qserv multinodes tests:

.. warning::
   The -all option below will remove any previous configuration for the same
   Qserv version.


.. code-block:: bash

   ./test.sh
