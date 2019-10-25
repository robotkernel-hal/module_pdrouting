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
            public robotkernel::trigger_base,
            public robotkernel::pd_provider,
            public robotkernel::pd_consumer,
            public service_provider::process_data_inspection::base
        {
            public:
                class output {
                    public: 
                        output(const std::string& name, const uint32_t& len) :
                            name(name), len(len), pdout(nullptr), pdtr(nullptr)
                        {
                        }

                        std::string name;
                        uint32_t len;
                        robotkernel::sp_process_data_t pdout;
                        robotkernel::sp_trigger_t      pdtr;
                        ssize_t hash;
                };

            private:
                std::shared_ptr<pdrouting> parent;
                std::list<output> outputs;
                std::string name; 

                struct {
                    std::string                     name;
                    ssize_t                         hash;
                    robotkernel::sp_process_data_t  dev;
                } pdin;

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

                //! trigger tick
                void tick();                
                
                // process data inspection
                void get_pdin(service_provider::process_data_inspection::pd_t& pd) {};
                void get_pdout(service_provider::process_data_inspection::pd_t& pd) {};
        };
        
        class pd_mux : 
            public std::enable_shared_from_this<pd_mux>,
            public robotkernel::trigger_base,
            public robotkernel::pd_provider,
            public robotkernel::pd_consumer,
            public service_provider::process_data_inspection::base
        {
            public:
                class input {
                    public: 
                        input(const std::string& name, const uint32_t& len) :
                            name(name), len(len), 
                            pdin(nullptr), pdtr(nullptr)
                        {
                        }

                        std::string name;
                        uint32_t len;
                        robotkernel::sp_process_data_t pdin;
                        robotkernel::sp_trigger_t      pdtr;
                        ssize_t hash;
                };

            private:
                std::shared_ptr<pdrouting> parent;
                std::list<input> inputs;

                struct {
                    std::string                     name;
                    ssize_t                         hash;
                    robotkernel::sp_process_data_t  dev;
                } pdout;

                std::string name; 
                std::string trigger_name;
                    
            public:
                //! construction
                /*!
                 * \param node yaml intialization node
                 */
                pd_mux(std::shared_ptr<pdrouting> parent, const YAML::Node& node);
                ~pd_mux() {};

                //! creating process data output and trigger
                void start();

                //! destroying process data output and trigger
                void stop();

                //! trigger tick
                void tick();                
                
                // process data inspection
                void get_pdin(service_provider::process_data_inspection::pd_t& pd) {};
                void get_pdout(service_provider::process_data_inspection::pd_t& pd) {};
        };

        typedef std::shared_ptr<pd_demux> sp_pd_demux_t;
        typedef std::list<sp_pd_demux_t> demux_list_t;
        demux_list_t demux;
        
        typedef std::shared_ptr<pd_mux> sp_pd_mux_t;
        typedef std::list<sp_pd_mux_t> mux_list_t;
        mux_list_t mux;

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

