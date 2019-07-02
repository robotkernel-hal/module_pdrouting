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

    pd_input_device_name = get_as<string>(node, "pd_input_device");

    for (const auto& output_node : node["outputs"]) {
        outputs.push_back(output(parent, get_as<string>(output_node, "name"), 
                    get_as<uint32_t>(output_node, "len")));
    }
}
                        
//! creating process data output and trigger
void pdrouting::pd_demux::start() {
    kernel& k = *kernel::get_instance();

    for (auto& output : outputs) {
        string tmp = format_string("%s.%s", parent->name.c_str(), output.name.c_str());
        output.pdtr  = make_shared<trigger>(tmp, "inputs");
        output.pdout = make_shared<triple_buffer>(output.len, tmp, string("inputs"), "", pdtr->id());
        output.hash  = output.pdout->set_provider(shared_from_this());

        k.add_device(output.pdtr);
        k.add_device(output.pdout);
    }
}

//! destroying process data output and trigger
void pdrouting::pd_demux::stop() {
    kernel& k = *kernel::get_instance();

    for (auto& output : outputs) {
        k.remove_device(output.pdout);
        k.remove_device(output.pdtr);

        output.pdout->reset_provider(shared_from_this());

        output.pdout = nullptr;
        output.pdtr  = nullptr;
        hash         = 0;
    }
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
    int ret = 0;


    return ret;
}


