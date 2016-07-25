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

//! trigger wrapper
static void pdrouting_trigger_wrapper(void *ptr) {
    pdrouting::pdroute *route = (pdrouting::pdroute *)ptr;

    if (route->out.mdl && route->trigger)
        route->out.mdl->trigger(route->out.slave_id);

    route->trigger_modules();
};
            
//! construction
/*!
 * \param node yaml intialization node
 */
pdrouting::pdroute::pdroute(pdrouting *parent, const YAML::Node& node) 
    : parent(parent) {
    slave_id = get_as<uint32_t>(node, "slave_id");
    trigger = get_as<bool>(node, "trigger", false);
    in.pd = out.pd = NULL;
    in.pd_len = out.pd_len = 0;
    in.mdl = out.mdl = NULL;
    pd_interface_id = NULL;

    if (node["in"]) {
        in.modname   = get_as<string>(node["in"], "modname");
        in.slave_id  = get_as<uint32_t>(node["in"], "slave_id");
        in.pd_offset = get_as<uint32_t>(node["in"], "pd_offset");
        in.pd_len    = get_as<uint32_t>(node["in"], "pd_len");
    }
    
    if (node["out"]) {
        out.modname   = get_as<string>(node["out"], "modname");
        out.slave_id  = get_as<uint32_t>(node["out"], "slave_id");
        out.pd_offset = get_as<uint32_t>(node["out"], "pd_offset");
        out.pd_len    = get_as<uint32_t>(node["out"], "pd_len");
    }
}

void pdrouting::pdroute::create_route(std::string base_mdl_name) {
    kernel& k = *kernel::get_instance();

    parent->log(info, "[module_pdrouting|%s] creating route slave_id %d\n",
            base_mdl_name.c_str(), slave_id);

    // direction inputs ===========
    if (in.pd_len > 0) { 
        // sanity check for module presence
        in.mdl = k.get_module(in.modname.c_str());
        if (!in.mdl)
            throw robotkernel::str_exception("[module_pdrouting|%s] module name "
                    "%s not found!\n", base_mdl_name.c_str(), in.modname.c_str());

        // add to module dependecies if not already in
        module *my_mdl = k.get_module(base_mdl_name.c_str());
        module::depend_list_t::const_iterator it;
        for (it = my_mdl->get_depends().begin(); it != my_mdl->get_depends().end(); ++it)
            if (*it == in.modname)
                break;
        if (it == my_mdl->get_depends().end())
            my_mdl->add_depends(in.modname);

        process_data_t pd; 
        pd.slave_id = in.slave_id;
        pd.pd = NULL;
        pd.len = 0;
        in.mdl->request(MOD_REQUEST_GET_PDIN, &pd);

        parent->log(info, "[module_pdrouting|%s]   got pdin %p/%d\n",
                base_mdl_name.c_str(), pd.pd, pd.len);

        if (pd.pd && (pd.len > (in.pd_offset + in.pd_len)))
            in.pd = (void *)((uint8_t *)pd.pd + in.pd_offset);

        parent->log(info, "[module_pdrouting|%s]   got pdin %p/%d\n",
                base_mdl_name.c_str(), in.pd, in.pd_len);
        
        // add trigger callback
        set_trigger_cb_t cb;
        cb.cb = pdrouting_trigger_wrapper;
        cb.hdl = this;
        cb.clk_id = in.slave_id;
        in.mdl->request(MOD_REQUEST_SET_TRIGGER_CB, &cb);
    }
    
    // direction outputs ===========
    if (out.pd_len > 0) {
        // sanity check for module presence
        out.mdl = k.get_module(out.modname.c_str());
        if (!out.mdl)
            throw robotkernel::str_exception("[module_pdrouting|%s] module name "
                    "%s not found!\n", base_mdl_name.c_str(), out.modname.c_str());

        // add to module dependecies if not already in
        module *my_mdl = k.get_module(base_mdl_name.c_str());
        module::depend_list_t::const_iterator it;
        for (it = my_mdl->get_depends().begin(); it != my_mdl->get_depends().end(); ++it)
            if (*it == out.modname)
                break;
        if (it == my_mdl->get_depends().end())
            my_mdl->add_depends(out.modname);

        process_data_t pd; 
        pd.slave_id = out.slave_id;
        pd.pd = NULL;
        pd.len = 0;
        out.mdl->request(MOD_REQUEST_GET_PDIN, &pd);

        if (pd.pd && (pd.len > (out.pd_offset + out.pd_len)))
            out.pd = (void *)((uint8_t *)pd.pd + out.pd_offset);
        
        parent->log(info, "[module_pdrouting|%s]   got pdout %p/%d\n",
                base_mdl_name.c_str(), out.pd, out.pd_len);
    }
    
    // add process data inspection 
    std::stringstream route_name; 
    route_name << "route_" << slave_id;

    YAML::Node node;
    node["mod_name"] = base_mdl_name;
    node["dev_name"] = route_name.str();
    node["slave_id"] = slave_id;
    node["loglevel"] = (string)parent->ll;
    pd_interface_id = robotkernel::kernel::register_interface_cb(
            "libinterface_process_data_inspection.so", node);
}

void pdrouting::pdroute::destroy_route(std::string base_mdl_name) {
    if (pd_interface_id)
        robotkernel::kernel::unregister_interface_cb(pd_interface_id);

    if (in.pd_len > 0) { 
        // sanity check for module presence
        kernel& k = *kernel::get_instance();
        module *mdl = k.get_module(in.modname.c_str());
        if (!mdl)
            throw robotkernel::str_exception("[module_pdrouting|%s] module name "
                    "%s not found!\n", base_mdl_name.c_str(), in.modname.c_str());
        
        // remove trigger callback
        set_trigger_cb_t cb;
        cb.cb = pdrouting_trigger_wrapper;
        cb.hdl = this;
        cb.clk_id = in.slave_id;
        mdl->request(MOD_REQUEST_UNSET_TRIGGER_CB, &cb);
    }

    in.pd = NULL;
    out.pd = NULL;
}

//! construction
/*!
 * \param node yaml intialization node
 */
pdrouting::pdrouting(const std::string& name, const YAML::Node& node) 
    : module_base("pdrouting", name, node) {
    for(unsigned i = 0; i < node.size(); ++i) {
        pdroute *p = new pdroute(this, node[i]);
        _routes[p->slave_id] = p;
    }
}

//! destruction 
pdrouting::~pdrouting() {
    set_state(module_state_init);

    route_map_t::iterator it;
    while ((it = _routes.begin()) != _routes.end()) {
        pdroute *r = it->second;
        _routes.erase(it);
        delete r;
    }
}
        
//! set module state machine to defined state
/*!
 * \param state requested state
 * \return success or failure
 */
int pdrouting::set_state(module_state_t state) {
    int ret = 0;

    switch (state) {
        case module_state_init:
        case module_state_preop: 
        case module_state_safeop: {
            route_map_t::iterator it;
            for (it = _routes.begin(); it != _routes.end(); ++it)
                it->second->destroy_route(name);
            break;
        }
        case module_state_op: {
            route_map_t::iterator it;
            for (it = _routes.begin(); it != _routes.end(); ++it)
                it->second->create_route(name);
            break;
        }
        default:
            ret = -1;
            break;
    }

    if (ret == 0)
        this->state = state;

    return ret;
}

//! send a request to module
/*!
 * \param reqcode request code
 * \param ptr pointer to request structure
 * \return success or failure
 */
int pdrouting::request(int reqcode, void* ptr) {
    int ret = 0;

    switch (reqcode) {
        case MOD_REQUEST_GET_PDIN: {            
            process_data_t *pd = (process_data_t *)ptr;
            pd->pd = NULL;
            pd->len = 0;

            if (_routes.find(pd->slave_id) != _routes.end()) {
                pd->pd = _routes[pd->slave_id]->in.pd;
                pd->len = _routes[pd->slave_id]->in.pd_len;
            }

            break;
        }
        case MOD_REQUEST_GET_PDOUT: {            
            process_data_t *pd = (process_data_t *)ptr;
            pd->pd = NULL;
            pd->len = 0;

            if (_routes.find(pd->slave_id) != _routes.end()) {
                pd->pd = _routes[pd->slave_id]->out.pd;
                pd->len = _routes[pd->slave_id]->out.pd_len;
            }

            break;
        }
        case MOD_REQUEST_SET_TRIGGER_CB: {
            set_trigger_cb_t *cb = (set_trigger_cb_t *)ptr;
            if (cb->cb == NULL) {
                log(error, "ERROR could not register, callback is NULL\n");
                break;
            }

            if (_routes.find(cb->clk_id) != _routes.end())
                _routes[cb->clk_id]->add_trigger_module(*cb);
            break;
        }
        case MOD_REQUEST_UNSET_TRIGGER_CB: {
            set_trigger_cb_t *cb = (set_trigger_cb_t *)ptr;

            if (cb->cb == NULL) {
                log(error, "ERROR could not remove, callback is NULL\n");
                break;
            }

            if (_routes.find(cb->clk_id) != _routes.end())
                _routes[cb->clk_id]->remove_trigger_module(*cb);
            break;
        }
        default:
            ret = -1;
            break;
    }

    return ret;
}

