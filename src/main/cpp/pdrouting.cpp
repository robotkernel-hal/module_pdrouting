//! robotkernel module pdrouting
/*!
 * author: Robert Burger
 *
 * $Id$
 */

// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab:

/*
 * This file is part of robotkernel.
 *
 * robotkernel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * robotkernel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with robotkernel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string_util/string_util.h>

#include "pdrouting.h"
#include "robotkernel/exceptions.h"
#include "robotkernel/helpers.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>

MODULE_DEF(pdrouting, module_pdrouting::pdrouting)

#define min(a, b) ((a) < (b) ? (a) : (b))
using namespace robotkernel;
using namespace std;
using namespace module_pdrouting;
using namespace string_util;

pdrouting::pd_demux::pd_demux(std::shared_ptr<pdrouting> parent, const YAML::Node& node) :
    pd_provider(format_string("%s.%s", parent->name.c_str(), get_as<string>(node, "name").c_str())),
    pd_consumer(format_string("%s.%s", parent->name.c_str(), get_as<string>(node, "name").c_str())),
    service_provider::process_data_inspection::base(parent->name, get_as<string>(node, "name")),
    parent(parent)
{
    /* we will get sth like:
      
        pd_input_device: <name>
        outputs:
        - { name: left, len: 8 }
        - { name: right, len: 8 }
    */

    pdin.name = get_as<string>(node, "pd_input_device");

    for (const auto& output_node : node["outputs"]) {
        outputs.push_back(output(get_as<string>(output_node, "name"), 
                    get_as<uint32_t>(output_node, "len")));
    }
}
                        
//! creating process data output and trigger
void pdrouting::pd_demux::start() {
    kernel& k = *kernel::get_instance();

    pdin.dev  = k.get_process_data(pdin.name);
    pdin.hash = pdin.dev->set_consumer(shared_from_this());

    for (auto& output : outputs) {
        string pd_desc = format_string("- uint8_t[%d]: data\n", output.len);
        string tmp = format_string("%s.%s", parent->name.c_str(), output.name.c_str());
        output.pdtr  = make_shared<trigger>(tmp, "inputs");
        output.pdout = make_shared<triple_buffer>(output.len, tmp, string("inputs"), pd_desc, output.pdtr->id());
        output.hash  = output.pdout->set_provider(shared_from_this());

        k.add_device(output.pdtr);
        k.add_device(output.pdout);
    }

    auto trigger_dev = k.get_trigger(pdin.dev->clk_device);
    trigger_dev->add_trigger(shared_from_this());
}

//! destroying process data output and trigger
void pdrouting::pd_demux::stop() {
    kernel& k = *kernel::get_instance();
    
    auto trigger_dev = k.get_trigger(pdin.dev->clk_device);
    trigger_dev->remove_trigger(shared_from_this());

    for (auto& output : outputs) {
        k.remove_device(output.pdout);
        k.remove_device(output.pdtr);

        output.pdout->reset_provider(output.hash);

        output.pdout = nullptr;
        output.pdtr  = nullptr;
        output.hash  = 0;
    }
    
    pdin.dev->reset_consumer(pdin.hash);
    pdin.hash = 0;
    pdin.dev  = nullptr;
}
                
//! trigger tick
void pdrouting::pd_demux::tick() {
    off_t pos = 0;
    auto buf = pdin.dev->pop(pdin.hash);

    for (auto& output : outputs) {
        output.pdout->write(output.hash, 0, &buf[pos], output.len);
        output.pdtr->trigger_modules();
        pos += output.len;
    }
}

pdrouting::pd_mux::pd_mux(std::shared_ptr<pdrouting> parent, const YAML::Node& node) :
    pd_provider(format_string("%s.%s", parent->name.c_str(), get_as<string>(node, "name").c_str())),
    pd_consumer(format_string("%s.%s", parent->name.c_str(), get_as<string>(node, "name").c_str())),
    service_provider::process_data_inspection::base(parent->name, get_as<string>(node, "name")),
    parent(parent)
{
    /* we will get sth like:
      
        pd_output_device: <name>
        trigger_name: <modname>.left.outputs.trigger
        inputs:
        - { name: left, len: 8 }
        - { name: right, len: 8 }
    */

    pdout.name = get_as<string>(node, "pd_output_device");
    trigger_name = get_as<string>(node, "trigger_name");

    for (const auto& input_node : node["inputs"]) {
        inputs.push_back(input(get_as<string>(input_node, "name"), 
                    get_as<uint32_t>(input_node, "len")));
    }
}
                        
//! creating process data input and trigger
void pdrouting::pd_mux::start() {
    kernel& k = *kernel::get_instance();

    pdout.dev  = k.get_process_data(pdout.name);
    pdout.hash = pdout.dev->set_consumer(shared_from_this());

    for (auto& input : inputs) {
        string pd_desc = format_string("- uint8_t[%d]: data\n", input.len);
        string tmp = format_string("%s.%s", parent->name.c_str(), input.name.c_str());
        input.pdtr  = make_shared<trigger>(tmp, "outputs");
        input.pdin  = make_shared<triple_buffer>(input.len, tmp, string("outputs"), pd_desc, input.pdtr->id());
        input.hash  = input.pdin->set_provider(shared_from_this());

        k.add_device(input.pdtr);
        k.add_device(input.pdin);
    }
    
    auto trigger_dev = k.get_trigger(trigger_name);
    trigger_dev->add_trigger(shared_from_this());
}

//! destroying process data input and trigger
void pdrouting::pd_mux::stop() {
    kernel& k = *kernel::get_instance();
    
    auto trigger_dev = k.get_trigger(trigger_name);
    trigger_dev->remove_trigger(shared_from_this());
    
    for (auto& input : inputs) {
        k.remove_device(input.pdin);
        k.remove_device(input.pdtr);

        input.pdin->reset_provider(input.hash);

        input.pdin  = nullptr;
        input.pdtr  = nullptr;
        input.hash  = 0;
    }
    
    pdout.dev->reset_consumer(pdout.hash);
    pdout.hash = 0;
    pdout.dev  = nullptr;
}
                
//! trigger tick
void pdrouting::pd_mux::tick() {
    off_t pos = 0;
    auto buf = pdout.dev->next(pdout.hash);

    for (auto& input : inputs) {
        auto buf = input.pdin->pop(input.hash);
        pdout.dev->write(pdout.hash, pos, buf, input.len, false);
        pos += input.len;
    }

    pdout.dev->push(pdout.hash);
}

//! construction
/*!
 * \param node yaml intialization node
 */
pdrouting::pdrouting(const std::string& name, const YAML::Node& node) : 
    module_base("module_pdrouting", name, node)
{
    config = YAML::Clone(node);
}

//! destruction 
pdrouting::~pdrouting() {
    set_state(module_state_init);
}

void pdrouting::init() {
    if (config["demux"]) {
        for (const auto& demux_node : config["demux"]) {
            auto d = std::make_shared<pd_demux>(shared_from_this(), demux_node);
            demux.push_back(d);
        }
    }
}
        
//! set module state machine to defined state
/*!
 * \param state requested state
 * \return success or failure
 */
int pdrouting::set_state(module_state_t state) {
    kernel& k = *kernel::get_instance();

    // get transition
    uint32_t transition = GEN_STATE(this->state, state);

    switch (transition) {
        case op_2_safeop:
        case op_2_preop:
        case op_2_init:
        case op_2_boot:
            // ====> stop sending commands
            for (auto& d : demux) {
                d->stop();
            }

            if (state == module_state_safeop)
                break;
        case safeop_2_preop:
        case safeop_2_init:
        case safeop_2_boot:
            // ====> stop receiving measurements
            if (state == module_state_preop)
                break;
        case preop_2_init:
        case preop_2_boot:
            // ====> deinit devices
        case init_2_init:
            // ====> re-/open ethercat device
            if (state == module_state_init)
                break;
        case init_2_boot:
            break;
        case boot_2_init:
        case boot_2_preop:
        case boot_2_safeop:
        case boot_2_op:
            // ====> re-/open ethercat device
            if (state == module_state_init)
                break;
        case init_2_op:
        case init_2_safeop:
        case init_2_preop:
            // ====> initial devices            
            if (state == module_state_preop)
                break;
        case preop_2_op:
        case preop_2_safeop: {
            // ====> start receiving measurements
            if (state == module_state_safeop)
                break;
        }
        case safeop_2_op: {
            // ====> start sending commands
            for (auto& d : demux) {
                d->start();
            }

            break;
        }
        case op_2_op:
        case safeop_2_safeop:
        case preop_2_preop:
            // ====> do nothing
            break;

        default:
            break;
    }

    return (this->state = state);
}


