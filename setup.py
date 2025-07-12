from setuptools import setup, Extension

setup(
    name="casyncio",
    version="0.1.0",
    ext_modules=[
        Extension(
            "casyncio",
            sources=["project/src/loopmodule.c"],
            include_dirs=["project/src"],
        )
    ],
)
