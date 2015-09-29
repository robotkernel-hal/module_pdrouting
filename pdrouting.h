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

#ifndef __MODULE_PDROUTING_H__
#define __MODULE_PDROUTING_H__

#include "robotkernel/module_intf.h"
#include "robotkernel/kernel.h"
#include "robotkernel/trigger_base.h"
#include "robotkernel/module_base.h"

namespace module_pdrouting {

class pdrouting 
    : public robotkernel::module_base {
    public:

        typedef struct pdroute : public robotkernel::trigger_base {
            //! construction
            /*!
             * \param node yaml intialization node
             */
            pdroute(pdrouting *parent, const YAML::Node& node);

            //! create route
            void create_route(std::string base_mdl_name);

            //! destroy route
            void destroy_route(std::string base_mdl_name);

            uint32_t slave_id;          //! virtual slave id
            bool trigger;               //! trigger out module on pd

            typedef struct pdinfo {
                std::string modname;    //! process data module name
                uint32_t slave_id;      //! slave id in pd module
                uint32_t pd_offset;     //! process data offset
                uint32_t pd_len;        //! process data length
                void *pd;               //! process data pointer
                robotkernel::module *mdl;
            } pdinfo_t;

            pdinfo_t in;                //! process data inputs
            pdinfo_t out;               //! process data outputs

            robotkernel::kernel::interface_id_t pd_interface_id;

            pdrouting *parent;
        } pdroute_t;

        typedef std::map<uint32_t, pdroute_t *> route_map_t;
        route_map_t _routes;

    public:
        //! construction
        /*!
         * \param node yaml intialization node
         */
        pdrouting(const std::string& name, const YAML::Node& node);

        //! destruction 
        ~pdrouting();

        //! set module state machine to defined state
        /*!
         * \param state requested state
         * \return success or failure
         */
        int set_state(module_state_t state);

        //! send a request to module
        /*!
         * \param reqcode request code
         * \param ptr pointer to request structure
         * \return success or failure
         */
        int request(int reqcode, void* ptr);
};

}; // namespace module_pdrouting

#endif // __MODULE_PDROUTING_H__

