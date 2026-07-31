# module_pdrouting

This robotkernel module provides process data (PD) routing.  It supports
three routing modes:

<dl>
  <dt>Multiplexing (mux)</dt>
  <dd>Many source process data devices are serialized consecutively into a
      single target process data device.  By default a trigger_collector
      fires the mux tick when all inputs have been triggered at least once.</dd>

  <dt>Demultiplexing (demux)</dt>
  <dd>One source process data device is split into many target process data
      devices at byte boundaries.  Each output consumes the next <code>len</code>
      bytes from the input stream.</dd>

  <dt>One-to-many (broadcast)</dt>
  <dd>The entire contents of one input PD device are distributed unchanged
      to multiple output PD devices.  All output PDs must have the same
      length as the input PD.</dd>
</dl>

## Configuration

For full option documentation see <a href="doc/pdrouting.rkc">doc/pdrouting.rkc</a>
and <a href="doc/pdrouting_with_classes.rkc">doc/pdrouting_with_classes.rkc</a>.

```yaml
# Configuration file for module_pdrouting.
#
# vi: set ft=yaml nowrap:
# -*- mode: yaml -*-

#########################################################
# Logging settings
# Standard robotkernel module local loglevel.
# Valid values: critical, error, warning, info, verbose, debug
#loglevel: verbose

#########################################################
# Process data demuxer configuration.
#
# Options: name, pd_input_device, zero_copy, trigger, trigger_name (deprecated),
#          outputs (list of name, len, desc)
demux:
- name: elmo_0_fsoe_demux
  pd_input_device: ethercat.slave_0.inputs.pd

  # Set to true to use zero-copy pointer buffers (advanced).
  #zero_copy: false

  # Explicit trigger (optional).  If omitted, the input PD's trigger
  # device is used automatically.
  #trigger:
  #  dev_name: timer.main.trigger
  #  prio: 50
  #  affinity: [ 2, 3 ]
  #  direct_mode: false

  outputs:
  - name: fsoe
    len: 11

  - name: axis
    len: 6

#########################################################
# Process data muxer configuration.
#
# Options: name, pd_output_device, expected_rate, zero_copy, trigger,
#          trigger_name (deprecated), inputs (list of name, len, desc)
mux:
- name: elmo_0_fsoe_mux
  pd_output_device: ethercat.slave_0.outputs.pd

  # Expected trigger rate for the automatic trigger_collector.
  #expected_rate: 1000

  # Set to true to use zero-copy pointer buffers (advanced).
  #zero_copy: false

  # Explicit trigger (optional).  If omitted, a trigger_collector is
  # used that fires when all inputs have been triggered at least once.
  #trigger:
  #  dev_name: timer.main.trigger
  #  prio: 50
  #  affinity: [ 2, 3 ]
  #  direct_mode: false

  inputs:
  - name: fsoe
    len: 11

  - name: axis
    len: 6

#########################################################
# Process data one-to-many (broadcast) configuration.
#
# Options: name, pd_input_device, pd_output_devices
one_to_many:
- name: slave_broadcast
  pd_input_device: ecat.slave_1.inputs.pd
  pd_output_devices:
  - ecat.slave_2.outputs.pd
  - ecat.slave_3.outputs.pd
```

## Process data

[[Category:robotkernel-5|pdrouting]]
