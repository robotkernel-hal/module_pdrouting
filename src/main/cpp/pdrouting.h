//! robotkernel module pdrouting
/*!
 * author: Robert Burger
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

#ifndef MODULE_PDROUTING_H
#define MODULE_PDROUTING_H

#include "robotkernel/module.h"
#include "robotkernel/module_base.h"
#include "robotkernel/kernel.h"
#include "robotkernel/trigger.h"
#include "robotkernel/process_data.h"

#include "service_provider/process_data_inspection/base.h"

namespace module_pdrouting {
#ifdef EMACS
}
#endif

class pdrouting :
    public std::enable_shared_from_this<pdrouting>,
    public robotkernel::module_base
{
    public:
        /*
         *  demux:
         *      pd_input_device: <name>
         *      outputs:
         *      -   name: left
         *          len: 8
         *      -   name: right
         *          len: 8
         *
         *
         */

        class pd_demux : 
            public std::enable_shared_from_this<pd_demux>,
            public robotkernel::pd_provider,
            public robotkernel::pd_consumer,
            public service_provider::process_data_inspection::base
        {
            public:
                class output {
                    public: 
                        output(std::shared_ptr<pdrouting> parent,
                                const std::string& name, const uint32_t& len) :
                            name(name), len(len), pdout(nullptr), pdtr(nullptr), parent(parent)
                        {
                        }

                        std::string name;
                        uint32_t len;
                        robotkernel::sp_process_data_t pdout;
                        robotkernel::sp_trigger_t      pdtr;
                        ssize_t hash;
            
                        std::shared_ptr<pdrouting> parent;
                };

            private:
                std::shared_ptr<pdrouting> parent;
                std::list<output> outputs;
                std::string pd_input_device_name;


            public:
                //! construction
                /*!
                 * \param node yaml intialization node
                 */
                pd_demux(std::shared_ptr<pdrouting> parent, const YAML::Node& node);
                ~pd_demux() {};

                //! creating process data output and trigger
                void start();

                //! destroying process data output and trigger
                void stop();

                // process data inspection
                void get_pdin(service_provider::process_data_inspection::pd_t& pd) {};
                void get_pdout(service_provider::process_data_inspection::pd_t& pd) {};
//            //! create route
//            void create_route(std::string base_mdl_name);
//
//            //! destroy route
//            void destroy_route(std::string base_mdl_name);
//
//            uint32_t slave_id;          //! virtual slave id
//            bool trigger;               //! trigger out module on pd
//
//            typedef struct pdinfo {
//                std::string modname;    //! process data module name
//                uint32_t slave_id;      //! slave id in pd module
//                uint32_t pd_offset;     //! process data offset
//                uint32_t pd_len;        //! process data length
//                void *pd;               //! process data pointer
//                robotkernel::module *mdl;
//            } pdinfo_t;
//
//            pdinfo_t in;                //! process data inputs
//            pdinfo_t out;               //! process data outputs
//
//            robotkernel::kernel::interface_id_t pd_interface_id;
//
//            pdrouting *parent;
        };

        typedef std::shared_ptr<pd_demux> sp_pd_demux_t;
        typedef std::list<sp_pd_demux_t> demux_list_t;
        demux_list_t demux;

        YAML::Node config;
    public:
        //! construction
        /*!
         * \param node yaml intialization node
         */
        pdrouting(const std::string& name, const YAML::Node& node);

        //! destruction 
        ~pdrouting();

        //! initializaion
        void init();

        //! set module state machine to defined state
        /*!
         * \param state requested state
         * \return success or failure
         */
        int set_state(module_state_t state);
};

#ifdef EMACS
{
#endif
}; // namespace module_pdrouting

#endif // MODULE_PDROUTING_H

