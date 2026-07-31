# module_pdrouting

Process data routing module for robotkernel.  It bridges hardware-level
process data (e.g. EtherCAT PDS/PDS) and application-level modules by
splitting, combining, or broadcasting PD streams.

## Routing Modes

### Demux — Split one PD into many

Takes a single input PD device and splits it at byte boundaries into
multiple output PD devices.  Typical use: an EtherCAT slave exposes a
single large input PD containing several logical data blocks (e.g. FSoE
payload + axis state); the demux exposes each block as its own PD device
so application modules can subscribe independently.

```
ethercat.slave_0.inputs.pd  (17 bytes)
  ├─ module_pdrouting.elmo_0_demux.fsoe.inputs.pd   (bytes  0-10)
  └─ module_pdrouting.elmo_0_demux.axis.inputs.pd    (bytes 11-16)
```

### Mux — Combine many PDs into one

Takes multiple input PD devices and serializes them consecutively into a
single output PD device.  Typical use: several application modules each
write to their own PD; the mux assembles them into the layout expected by
a hardware output (e.g. EtherCAT PDO).

```
app_fsoe.outputs.pd   (11 bytes) ─┐
                                   ├─→ ethercat.slave_0.outputs.pd  (17 bytes)
app_axis.outputs.pd    (6 bytes)  ─┘
```

### One-to-Many — Broadcast one PD to many

Copies the entire contents of one input PD to multiple output PDs
unchanged.  All outputs must have the same length as the input.
Typical use: distributing a master's sensor data to several slave
controllers simultaneously.

```
ecat.master.inputs.pd  ──→ ecat.slave_0.outputs.pd
                 ──→ ecat.slave_1.outputs.pd
                 ──→ ecat.slave_2.outputs.pd
```

## Generated Devices

For each demux output and mux input the module automatically creates:

| Device | Path pattern | Purpose |
|--------|-------------|---------|
| Process data | `<module>.<name>.<entry>.{inputs,outputs}.pd` | The routed PD |
| PD inspection | same path | Runtime type & value inspection |

The module also registers PD type definitions at
`<module>.<name>.<entry>.{inputs,outputs}.definition` so consumers can
query the data layout.

## PD Type Descriptions

By default the module auto-generates the PD type description for each
routed entry by walking the source PD's YAML definition and matching
field sizes against the configured `len`.  If the split aligns cleanly
with field boundaries, the generated description preserves the original
field names and types.  If the split falls in the middle of a field,
generation is aborted and the entry falls back to a raw
`uint8_t[]` array of the requested length.

To override the auto-generated description, supply an explicit `desc`
field on the entry.

## Trigger Behavior

| Mode | Default | Override |
|------|---------|----------|
| **Demux** | Input PD's trigger device | `trigger:` map or `trigger_name:` (deprecated) |
| **Mux** | `trigger_collector` — fires when all inputs have triggered | `trigger:` map or `trigger_name:` (deprecated) |
| **One-to-many** | Input PD's trigger device | (not configurable) |

The `trigger:` map supports `dev_name`, `prio`, `affinity`, and
`direct_mode`.  When a mux uses the default `trigger_collector`, the
`expected_rate` option (default: 1 Hz) controls missed-trigger detection.

## Zero-Copy Mode

Setting `zero_copy: true` on a demux or mux replaces the default
`triple_buffer` PDs with `pointer_buffer` PDs, eliminating one memcpy
per tick.  Trade-offs:

- **Demux:** input data is copied once into a persistent internal buffer;
  output pointers reference that buffer.  Safe as long as consumers do
  not hold pointers across ticks.
- **Mux:** input pointers reference the output PD's internal buffer
  directly.  Producers write into the output buffer via the input PDs.

## Configuration Reference

| Section | Option | Type | Required | Default | Description |
|---------|--------|------|----------|---------|-------------|
| **demux** | `name` | string | yes | — | Demuxer name (device path prefix) |
| | `pd_input_device` | string | yes | — | Source PD device to split |
| | `zero_copy` | bool | no | `false` | Use pointer_buffer for outputs |
| | `trigger` | map | no | — | Explicit trigger (`dev_name`, `prio`, `affinity`, `direct_mode`) |
| | `trigger_name` | string | no | — | Legacy trigger (deprecated) |
| | `outputs[].name` | string | yes | — | Output entry infix name |
| | `outputs[].len` | int | yes | — | Byte count for this output |
| | `outputs[].desc` | string | no | — | Explicit PD type definition YAML |
| **mux** | `name` | string | yes | — | Muxer name (device path prefix) |
| | `pd_output_device` | string | yes | — | Target PD device to write to |
| | `expected_rate` | int | no | `1` | Expected trigger rate in Hz (trigger_collector) |
| | `zero_copy` | bool | no | `false` | Use pointer_buffer for inputs |
| | `trigger` | map | no | — | Explicit trigger (`dev_name`, `prio`, `affinity`, `direct_mode`) |
| | `trigger_name` | string | no | — | Legacy trigger (deprecated) |
| | `inputs[].name` | string | yes | — | Input entry infix name |
| | `inputs[].len` | int | yes | — | Byte count this input contributes |
| | `inputs[].desc` | string | no | — | Explicit PD type definition YAML |
| **one_to_many** | `name` | string | no | — | Name used in log messages |
| | `pd_input_device` | string | yes | — | Source PD device to broadcast |
| | `pd_output_devices` | list | yes | — | Target PD device names |

Each top-level section (`demux`, `mux`, `one_to_many`) accepts either a
plain YAML sequence or a `classes`/`instances` map for robotkernel
template expansion.

## Examples

Complete, well-commented configuration examples:

- **[doc/pdrouting.rkc](doc/pdrouting.rkc)** — all three routing modes
- **[doc/pdrouting_with_classes.rkc](doc/pdrouting_with_classes.rkc)** — using class/instance templates for bulk configuration

[[Category:robotkernel-5|pdrouting]]
