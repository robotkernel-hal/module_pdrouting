from conan import ConanFile

class MainProject(ConanFile):
    python_requires = "conan_template/[~6]@robotkernel/stable"
    python_requires_extend = "conan_template.RobotkernelConanFile"

    name = "module_pdrouting"
    url = "https://rmc-github.robotic.dlr.de/robotkernel/module_pd_routing.git"
    description = "pd routing is used to mux or demux process data to/from other pd's"
    exports_sources = ["*", "!.gitignore"]

    tool_requires = ["robotkernel_service_helper/[~6]@robotkernel/stable"]
    
    def requirements(self):
        self.requires("robotkernel/[~6]@robotkernel/stable")
        self.requires("service_provider_process_data_inspection/[~6]@robotkernel/stable")
