# module_pdrouting

This robotkernel module does process data routing. It enables handles process data devices in two connection methods. 

<dl>
  <dt>Multiplexing</dt>
  <dd>With multiplexing the modules enables that many source process data devices are *multiplexed* into one target process data device. For this the source pd's are serialized consecutively into a new or arleady exting target pd.</dd>

  <dt>Demultiplexing</dt>
  <dd>The opposite method of multiplexing. Here one source process data devices is splitted into many target process data devices. It it always splitted at byte boundaries</dd>
</dl>

## Configuration

```yaml
# Configuration file for module_pdrouting.
#
# vi: set ft=yaml nowrap:
# -*- mode: yaml -*-

#########################################################
# logging settings

# Standard robotkernel module local loglevel.
#loglevel: verbose

#########################################################
# Process data demuxer configuration.
demux:
- # Demuxer name (prefix for demuxed process data).
  name: elmo_0_fsoe_demux

  # Process data device to demux.
  pd_input_device: ethercat.slave_0.inputs.pd

  # Demux data outputs.
  outputs:
  - # Demuxed process data infix name.
    name: fsoe

    # Demuxing next <len> bytes.
    len: 11

  - # Demuxed process data infix name.
    name: axis

    # Demuxing next <len> bytes.
    len: 6 

#########################################################
# Process data Muxer configuration.
mux:
- # Muxer name (prefix for muxed process data)
  name: elmo_0_fsoe_mux

  # Process data device to mux
  pd_output_device: ethercat.slave_0.outputs.pd

  # Trigger device to be trigger when all muxing devices have data.
  trigger_name: ethercat.slave_0.inputs.trigger
  
  # Mux data outputs.
  outputs:
  - # Muxed process data infix name.
    name: fsoe

    # Muxing next <len> bytes.
    len: 11

  - # Muxed process data infix name.
    name: axis

    # Muxing next <len> bytes.
    len: 6 
```

## Process data 

[[Category:robotkernel-5|pdrouting]]
