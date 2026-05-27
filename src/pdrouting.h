//! robotkernel module pdrouting
/*!
 * author: Robert Burger
 */

// vim: tabstop=4 softtabstop=4 shiftwidth=4 expandtab:

/*
 * This file is part of module_pdrouting.
 *
 * module_pdrouting is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 * 
 * module_pdrouting is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with module_pdrouting; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef MODULE_PDROUTING_H
#define MODULE_PDROUTING_H

#include "robotkernel/module_base.h"
#include "robotkernel/robotkernel.h"
#include "robotkernel/trigger.h"
#include "robotkernel/trigger_collector.h"
#include "robotkernel/process_data.h"

#include "service_provider_process_data_inspection/base.h"

namespace module_pdrouting {

struct pd {
    std::string                     name;
    std::string                     trigger_name;
    robotkernel::sp_process_data_t  dev;
    robotkernel::sp_pd_provider_t   provider;
    robotkernel::sp_pd_consumer_t   consumer;
}; 

class pdrouting :
    public virtual robotkernel::shared_base,
    public robotkernel::module_base
{
    public:
        /* 
         * one_to_many:
         *     pd_input_device: <name>
         *     pd_output_devices:
         *     -   <output_1>
         *     -   <output_2>
         *     -   <output_3>
         *     -   <output_4>
         */
        class one_to_many :
            public virtual robotkernel::shared_base,
            public robotkernel::trigger_base
        {
            public:
                std::string name;
                std::shared_ptr<pdrouting> parent;
                struct pd pdin;
                std::list<struct pd> pdout;

            public:
                one_to_many(std::shared_ptr<pdrouting> parent, const YAML::Node& node);
                ~one_to_many();
                
                //! creating process data output and trigger
                void start();

                //! destroying process data output and trigger
                void stop();

                //! trigger tick
                void tick();                
        };

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
            public virtual robotkernel::shared_base,
            public robotkernel::trigger_base
        {
            public:
                class output {
                    public: 
                        output(const std::string& name, const uint32_t& len, const std::string& desc = "") :
                            name(name), len(len), desc(desc), pdout(nullptr)
                        {
                        }

                        std::string name;
                        uint32_t len;
                        std::string desc;
                        std::string gen_desc;
                        robotkernel::sp_process_data_t pdout;
                        robotkernel::sp_pd_provider_t provider;
                        service_provider_process_data_inspection::sp_pd_inspection_t pdout_inspection;
                };

            private:
                std::shared_ptr<pdrouting> parent;
                std::list<output> outputs;
                std::string name; 
                struct pd pdin;
                bool zero_copy = false;

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
        };
        
        class pd_mux : 
            public virtual robotkernel::shared_base,
            public robotkernel::trigger_base
        {
            public:
                class input {
                    public: 
                        input(const std::string& name, const uint32_t& len, const std::string& desc = "") :
                            name(name), len(len), desc(desc),
                            pdin(nullptr)
                        {
                        }

                        std::string name;
                        uint32_t len;
                        std::string desc;
                        std::string gen_desc;
                        robotkernel::sp_process_data_t pdin;
                        robotkernel::sp_pd_consumer_t consumer;
                        service_provider_process_data_inspection::sp_pd_inspection_t pdin_inspection;
                        robotkernel::sp_trigger_cb_t collector_trigger_cb;
                };

            private:
                std::shared_ptr<pdrouting> parent;
                std::vector<input> inputs;
                struct pd pdout;

                std::string name; 
                std::string trigger_name;
                double expected_rate;
                bool zero_copy = false;
                    
                robotkernel::sp_trigger_t collector_trigger;
                robotkernel::sp_trigger_collector_t collector;
                
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
        };
        
#if 0 
        class pd_mux_merge :
            public virtual robotkernel::shared_base,
            public robotkernel::trigger_base
        {
            private:
                std::shared_ptr<pdrouting> parent;
                std::vector<struct pd> pdins;
                struct pd pdout;

                robotkernel::sp_trigger_t collector_trigger;
                robotkernel::sp_trigger_collector_t collector;
                
            public:

                //! construction
                /*!
                 * \param node yaml intialization node
                 */
                pd_mux_merge(std::shared_ptr<pdrouting> parent, const YAML::Node& node)
                {
                    pdout.name = robotkernel::helpers::get_as<std::string>(node, "pd_output_device");
                    
                    for (const auto& input_dev_name : node["pd_input_devices"]) {
                        struct pd tmp;
                        tmp.name = input_dev_name.as<std::string>();
                        pdins.push_back(tmp);
                    }
                }

                ~pd_mux_merge() {};

                //! creating process data output and trigger
                void start()        
                {
                    try {
                        pdout.dev = robotkernel::get_device<robotkernel::process_data>(pdout.name);
                    } catch (std::exception& e) {
                        // pdout device does not exist, create one with our prefix!
                        pdout.dev = std::make_shared<robotkernel::triple_buffer>(parent->name, pdout.name, "");
                    }
                }

                //! destroying process data output and trigger
                void stop();

                //! trigger tick
                void tick();                
        };
#endif

        typedef std::shared_ptr<pd_demux> sp_pd_demux_t;
        typedef std::list<sp_pd_demux_t> demux_list_t;
        demux_list_t demux;
        
        typedef std::shared_ptr<pd_mux> sp_pd_mux_t;
        typedef std::list<sp_pd_mux_t> mux_list_t;
        mux_list_t mux;

        typedef std::shared_ptr<one_to_many> sp_o2m_t;
        typedef std::list<sp_o2m_t> o2m_list_t;
        o2m_list_t o2m;

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

}; // namespace module_pdrouting

#endif // MODULE_PDROUTING_H

