.. _clint:

############################
Core Local Interrupt (CLINT)
############################

This chapter will provide details on the Core Local Interrupt (CLINT) controller instantiated
in this design. CLINT is responsible for maintaining memory mapped control and status registers
which are associated with the software and timer interrupts. The spec presented here is compatible
with the *RISC-V Privileged Architecture Version 1.10*


IP Details and Available Configuration
======================================


:numref:`CLINT_ip_details` provides details of the source of the IP and and
details of the memory map.

.. tabularcolumns:: |l|C|

.. _CLINT_ip_details:

.. table:: CLINT IP details

  ========================================  ==============
  ..                                        **Value**
  ========================================  ==============
  Provider                                  gitlab
  Vendor                                    incoresemi
  Library                                   blocks/devices
  Version                                   1.1.2
  Ip Type                                   memory_mapped
  Numer of Config Registers                 3
  Direct Memory Region                      None
  Configuration Register Alignment (bytes)  8
  ========================================  ==============

:numref:`CLINT_configuration_details` provides information of the various
parameters of the IP available at design time and their description

.. tabularcolumns:: |l|l|l|

.. _CLINT_configuration_details:

.. table:: CLINT IP Configuration Options

  ===============  =====================  ======================================================================================================================================================================================
  Configuration    Options                Description
  ===============  =====================  ======================================================================================================================================================================================
  Bus interfaces   APB, AXI4L, AXI4       Choice of bus interface protocol supported on this IP
  Base address     Integer                The base address where the memory map of the configuration register starts
  Bytes reserved   Integer >= 0XBFFF      The number of bytes reserved for this instance. This can be much larger than the actual bytes required for the configuration registers but cannot be smaller than the value indicated.
  tick_count       Integer > 1            defines the number of clocks cyles required to increment the mtime register by 1.
  msip             Integer > 1 and  < 32  defines the size of the msip register. Each bit is typically required to interrupt a separate hart
  ===============  =====================  ======================================================================================================================================================================================



CLINT Instance Details
======================



:numref:`CLINT_instance_details` shows the values assigned to parameters of this
instance of the IP.

.. tabularcolumns:: |c|C|

.. _CLINT_instance_details:

.. table:: CLINT Instance Parameters and Assigned Values

  ====================  ====================
  **Parameter Name**    **Value Assigned**
  ====================  ====================
  Base Address          0X2000000
  Bound Address         0X200BFFF
  Bytes reserved        0XBFFF
  Bus Interface         AXI4L
  tick_count            0X100
  msip                  0X1
  ====================  ====================


Register Map
============


The register map for the CLINT control registers is shown in
:numref:`CLINT_register_map`. 

.. tabularcolumns:: |l|c|c|c|l|

.. _CLINT_register_map:

.. table:: CLINT Register Mapping for all configuration registes

  +-----------------+---------------+--------------+--------------+--------------------------------------------------------------------+
  | Register-Name   | Offset(hex)   |   Size(Bits) | Reset(hex)   | Description                                                        |
  +=================+===============+==============+==============+====================================================================+
  | msip            | 0X0           |           32 | 0X0          | This register generates machine mode software interrupts when set. |
  +-----------------+---------------+--------------+--------------+--------------------------------------------------------------------+
  | mtimecmp        | 0X4000        |           64 | 0X0          | This register holds the compare value for the timer.               |
  +-----------------+---------------+--------------+--------------+--------------------------------------------------------------------+
  | mtime           | 0XBFF8        |           64 | 0X0          | Provides the current timer value.                                  |
  +-----------------+---------------+--------------+--------------+--------------------------------------------------------------------+

All addresses not mentioned in the above table within ``Base Address`` and
``Bound Address`` are reserved and accesses to those regions will generate a
slave error on the bus interface





The register access attributes for the CLINT control registers are shown in 
:numref:`CLINT_register_access_attr`.

.. tabularcolumns:: |l|c|c|c|C|

.. _CLINT_register_access_attr:

.. table:: CLINT Register Access Attributes for all configuration Registers

  ===============  =============  ============  ============  ============
  Register-Name    Access Type    Reset Type    Min Access    Max Access
  ===============  =============  ============  ============  ============
  msip             read-write     synchronous   1B            8B
  mtimecmp         read-write     synchronous   1B            8B
  mtime            read-write     synchronous   1B            8B
  ===============  =============  ============  ============  ============

:numref:`CLINT_register_sideeffects` captures the side-effects caused to either reads or writes
on certain registers.

.. tabularcolumns:: |l|l|l|

.. _CLINT_register_sideeffects:

.. table:: CLINT Register Side Effects

  ===============  ===================  ===============================================
  Register-Name    Read Side Effects    Write Side Effects
  ===============  ===================  ===============================================
  mtimecmp         none                 Writing to register clears the timer interrupt.
  ===============  ===================  ===============================================

.. note:: Registers not included in the Side Effects table have no side-effects generated
  either on a read or a write.






MSIP Register
=============

This register generates machine mode software interrupts when set.

.. bitfield::
    :bits: 32
    :lanes: 4
    :fontsize: 10
    :vspace: 50
    :hspace: 1200

    [
    {"bits": 1, "name": "MSIP", "attr":"read-write" },
    {"bits": 31, "name": "Reserved", "attr":"" ,"type": 0}]




.. tabularcolumns:: |l|l|l|l|

.. _msip_subfields:

.. table:: msip subfeild description

  +--------+--------------+-------------+----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
  | Bits   | Field Name   | Attribute   | Description                                                                                                                                                                                                                                                                                                                                                                                                                |
  +========+==============+=============+============================================================================================================================================================================================================================================================================================================================================================================================================================+
  | [0:0]  | msip         | read-write  | Machine-mode software interrupts are generated by writing to the memory-mapped control register ``msip`` . The ``msip`` register is a 32-bit wide WARL register where the upper 31 bits are tied to 0. The least significant bit can be used to drive the ``MSIP`` bit of the ``mip`` CSR of a RISC-V hart. Other bits in the ``msip`` register are hardwired to zero. On reset, the ``msip`` register is cleared to zero. |
  +--------+--------------+-------------+----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+
  | [31:1] | Reserved     | read-write  | Reads will return zeros and writes will have no effect                                                                                                                                                                                                                                                                                                                                                                     |
  +--------+--------------+-------------+----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------+





MTIMECMP Register
=================

This is a read-write register and holds a 64-bit value. A timer interrupt is pending whenever ``mtime`` is greater than or equal to the value in the ``mtimecmp`` register. The timer interrupt is used to drive the ``MTIP`` bit of the ``mip`` CSR of a RISC-V core. 

.. bitfield::
    :bits: 64
    :lanes: 1
    :fontsize: 10
    :vspace: 50
    :hspace: 1200

    [
    {"bits": 64, "name": "MTIMECMP", "attr":"read-write" }]


MTIME Register
==============

``mtime`` is a 64-bit read-write register that keeps track of the number of cycles counted from an arbitrary point in time. It is a free-running counter which is incremented every ``tick_count`` number of cycles

.. bitfield::
    :bits: 64
    :lanes: 1
    :fontsize: 10
    :vspace: 50
    :hspace: 1200

    [
    {"bits": 64, "name": "MTIME", "attr":"read-write" }]



IO and Sideband Signals
=======================





.. tabularcolumns:: |l|l|l|l|

.. _CLINT_sb_signals:

.. table:: CLINT generated side-band signals generated

  ===================  ======  ===========  =========================================================================================================
  Signal Name (RTL)      Size  Direction    Description
  ===================  ======  ===========  =========================================================================================================
  sb_clint_msip             1  output       Drive the MSIP bit in *mip* CSR of corresponding RISC-V cores. Indicates a software interrupt is pending.
  sb_clint_mtip             1  output       Drive the MTIP bit in *mip* CSR of RISC-V cores. Indicates a timer interrupt is pending.
  sb_clint_mtime           64  output       Holds the current value of the *mtime* register. Used as a shadow for TIME csr in a RISC-V core.
  ===================  ======  ===========  =========================================================================================================

