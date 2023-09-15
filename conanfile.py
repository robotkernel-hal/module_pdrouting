from conans import tools, python_requires

base = python_requires("conan_template/[~=5]@robotkernel/stable")

class MainProject(base.RobotkernelConanFile):
    name = "module_pdrouting"
    description = "pd routing is used to mux or demux process data to/from other pd's"
    exports_sources = ["*", "!.gitignore"] + ["!%s" % x for x in tools.Git().excluded_files()]
    requires = (
            "robotkernel/5.0.44@robotkernel/unstable",
            "service_provider_process_data_inspection/[~=5]@robotkernel/stable" )

