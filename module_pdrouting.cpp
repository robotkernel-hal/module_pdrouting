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

#include "robotkernel/exceptions.h"
#include <string_util/string_util.h>

#undef BUILD_USER
#undef BUILD_DATE
#undef BUILD_HOST
#undef PACKAGE
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef VERSION

#include "module_pdrouting.h"
#include "config.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iostream>

#define min(a, b) ((a) < (b) ? (a) : (b))
using namespace robotkernel;
using namespace std;

//! log to kernel logging facility
void pdrouting::mlog(robotkernel::loglevel lvl, const char *format, ...) {
    char buf[1024];

    // format argument list
    va_list args;
    va_start(args, format);
    vsnprintf(buf, 1024, format, args);
    klog(lvl, "[module_pdrouting|%s] %s", _name.c_str(), buf);
}
            
//! construction
/*!
 * \param node yaml intialization node
 */
pdrouting::pdroute::pdroute(const YAML::Node& node) {
    slave_id = node["slave_id"].to<uint32_t>();
    pd_interface_id = NULL;
    in.pd = out.pd = NULL;
    in.pd_len = out.pd_len = 0;

    const YAML::Node *in_node = node.FindValue("in");
    if (in_node) {
        in.modname   = (*in_node)["modname"].to<string>();
        in.slave_id  = (*in_node)["slave_id"].to<uint32_t>();
        in.pd_offset = (*in_node)["pd_offset"].to<uint32_t>();
        in.pd_len    = (*in_node)["pd_len"].to<uint32_t>();
    }
    
    const YAML::Node *out_node = node.FindValue("out");
    if (out_node) {
        out.modname   = (*out_node)["modname"].to<string>();
        out.slave_id  = (*out_node)["slave_id"].to<uint32_t>();
        out.pd_offset = (*out_node)["pd_offset"].to<uint32_t>();
        out.pd_len    = (*out_node)["pd_len"].to<uint32_t>();
    }
}

void pdrouting::pdroute::create_route(std::string base_mdl_name) {
    kernel& k = *kernel::get_instance();

    klog(module_info, "[module_pdrouting|%s] creating route slave_id %d\n",
            base_mdl_name.c_str(), slave_id);

    // direction inputs ===========
    if (in.pd_len > 0) { 
        // sanity check for module presence
        module *mdl = k.get_module(in.modname.c_str());
        if (!mdl)
            throw robotkernel::str_exception("[module_pdrouting|%s] module name "
                    "%s not found!\n", base_mdl_name.c_str(), in.modname.c_str());

        // add to module dependecies if not already in
        module *my_mdl = k.get_module(base_mdl_name.c_str());
        module::depend_list_t::iterator it;
        for (it = my_mdl->depends.begin(); it != my_mdl->depends.end(); ++it)
            if (*it == in.modname)
                break;
        if (it == my_mdl->depends.end())
            my_mdl->depends.push_back(in.modname);

        process_data_t pd; 
        pd.slave_id = in.slave_id;
        pd.pd = NULL;
        pd.len = 0;
        mdl->request(MOD_REQUEST_GET_PDIN, &pd);

        klog(module_info, "[module_pdrouting|%s]   got pdin %p/%d\n",
                base_mdl_name.c_str(), pd.pd, pd.len);

        if (pd.pd && (pd.len > (in.pd_offset + in.pd_len)))
            in.pd = (void *)((uint8_t *)pd.pd + in.pd_offset);

        klog(module_info, "[module_pdrouting|%s]   got pdin %p/%d\n",
                base_mdl_name.c_str(), in.pd, in.pd_len);
    }
    
    // direction outputs ===========
    if (out.pd_len > 0) {
        // sanity check for module presence
        module *mdl = k.get_module(out.modname.c_str());
        if (!mdl)
            throw robotkernel::str_exception("[module_pdrouting|%s] module name "
                    "%s not found!\n", base_mdl_name.c_str(), out.modname.c_str());

        // add to module dependecies if not already in
        module *my_mdl = k.get_module(base_mdl_name.c_str());
        module::depend_list_t::iterator it;
        for (it = my_mdl->depends.begin(); it != my_mdl->depends.end(); ++it)
            if (*it == out.modname)
                break;
        if (it == my_mdl->depends.end())
            my_mdl->depends.push_back(out.modname);

        process_data_t pd; 
        pd.slave_id = out.slave_id;
        pd.pd = NULL;
        pd.len = 0;
        mdl->request(MOD_REQUEST_GET_PDIN, &pd);

        if (pd.pd && (pd.len > (out.pd_offset + out.pd_len)))
            out.pd = (void *)((uint8_t *)pd.pd + out.pd_offset);
        
        klog(module_info, "[module_pdrouting|%s]   got pdout %p/%d\n",
                base_mdl_name.c_str(), out.pd, out.pd_len);
    }
    
    // add process data inspection 
    std::stringstream route_name; 
    route_name << "route_" << slave_id;
    pd_interface_id = robotkernel::kernel::register_interface_cb(base_mdl_name.c_str(), 
            "libinterface_process_data_inspection.so", route_name.str().c_str(), slave_id);
}

void pdrouting::pdroute::destroy_route(std::string base_mdl_nam) {
    if (pd_interface_id)
        robotkernel::kernel::unregister_interface_cb(pd_interface_id);

    in.pd = NULL;
    out.pd = NULL;
}

//! construction
/*!
 * \param node yaml intialization node
 */
pdrouting::pdrouting(const std::string& name, const YAML::Node& node) {
    _name       = name;

    for(unsigned i = 0; i < node.size(); ++i) {
        pdroute *p = new pdroute(node[i]);
        _routes[p->slave_id] = p;
    }

    set_state(module_state_init);
}

//! destruction 
pdrouting::~pdrouting() {
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
                it->second->destroy_route(_name);
            break;
        }
        case module_state_op: {
            route_map_t::iterator it;
            for (it = _routes.begin(); it != _routes.end(); ++it)
                it->second->create_route(_name);
            break;
        }
        default:
            ret = -1;
            break;
    }

    if (ret == 0)
        _state = state;

    return ret;
}

//! get module state machine state
/*!
 * \return current state
 */
module_state_t pdrouting::get_state() {
    return _state;
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
                mlog(module_error, "ERROR could not register, callback is NULL\n");
                break;
            }

            if (_routes.find(cb->clk_id) != _routes.end())
                _routes[cb->clk_id]->add_trigger_module(*cb);
            break;
        }
        case MOD_REQUEST_UNSET_TRIGGER_CB: {
            set_trigger_cb_t *cb = (set_trigger_cb_t *)ptr;

            if (cb->cb == NULL) {
                mlog(module_error, "ERROR could not remove, callback is NULL\n");
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

//! module trigger callback
void pdrouting::trigger() {
}

#ifdef __cplusplus
extern "C" {
#endif
#if 0
}
#endif

//! cyclic process data read
/*!
  \param hdl module handle
  \param buf process data buffer 
  \param bufsize size of process data buffer
  \return size of read bytes
 */
size_t mod_read(MODULE_HANDLE hdl, void* buf, size_t bufsize) {
    return 0;
}

//! cyclic process data write
/*!
  \param hdl module handle
  \param buf process data buffer
  \param bufsize size of process data buffer 
  \return size of written bytes
 */
size_t mod_write(MODULE_HANDLE hdl, void* buf, size_t bufsize) {
    return 0;
}

//! configures module
/*!
  \param name module name
  \param config configure string
  \return handle on success, NULL otherwise
*/
MODULE_HANDLE mod_configure(const char* name, const char* config) {
    pdrouting *pdrouting_dev;

    // open config
    std::stringstream stream(config);
    YAML::Parser parser(stream);
    YAML::Node doc;

    klog(module_info, "[module_pdrouting|%s] build by: %s@%s\n", 
            name, BUILD_USER, BUILD_HOST);
    klog(module_info, "[module_pdrouting|%s] build date: %s\n", 
            name, BUILD_DATE);

    if (!parser.GetNextDocument(doc)) {
        klog(module_error, "[module_pdrouting|%s] parsing config file\n", name);
        return (MODULE_HANDLE)NULL;
    }
    
    pdrouting_dev = new pdrouting(name, doc);
    if (!pdrouting_dev) {
        klog(module_error, "[module_pdrouting|%s] cannot allocate memory", name);
        return (MODULE_HANDLE)NULL;
    }

    return (MODULE_HANDLE)pdrouting_dev;
}

//! unconfigure module
/*!
  \param hdl module handle
  \return success or failure
 */
int mod_unconfigure(MODULE_HANDLE hdl) {
    pdrouting *pdrouting_dev = (pdrouting *)hdl;
    if (!pdrouting_dev) {
        errno = EINVAL;
        return -1;
    }

    delete pdrouting_dev;
    return 0;
}

//! set module state machine to defined state
/*!
  \param hdl module handle
  \param state requested state
  \return success or failure
 */
int mod_set_state(MODULE_HANDLE hdl, module_state_t state) {
    pdrouting *pdrouting_dev = (pdrouting *)hdl;
    if (!pdrouting_dev) {
        errno = EINVAL;
        return -1;
    }

    return pdrouting_dev->set_state(state);
}

//! get module state machine state
/*!
  \param hdl module handle
  \return current state
 */
module_state_t mod_get_state(MODULE_HANDLE hdl) {
    pdrouting *pdrouting_dev = (pdrouting *)hdl;
    if (!pdrouting_dev) {
        errno = EINVAL;
        return module_state_unknown;
    }

    return pdrouting_dev->get_state();
}

//! send a request to module
/*!
  \param hdl module handle
  \param reqcode request code
  \param ptr pointer to request structure
  \return success or failure
 */
int mod_request(MODULE_HANDLE hdl, int reqcode, void* ptr) {
    pdrouting *pdrouting_dev = (pdrouting *)hdl;
    if (!pdrouting_dev) {
        errno = EINVAL;
        return -1;
    }

    return pdrouting_dev->request(reqcode, ptr);
}

//! module trigger callback
/*!
 * \param hdl module handle
 */
void mod_trigger(MODULE_HANDLE hdl) {
    pdrouting *pdrouting_dev = (pdrouting *)hdl;
    if (!pdrouting_dev) {
        errno = EINVAL;
        return;
    }

    pdrouting_dev->trigger();
}

#if 0
{
#endif
#ifdef __cplusplus
}
#endif

