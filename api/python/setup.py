#!/usr/bin/env python3
"""
Setup script for Lumitec Poco CAN API Python library
"""

from setuptools import setup, find_packages

# Read README file
try:
    with open("README.md", "r", encoding="utf-8") as fh:
        long_description = fh.read()
except FileNotFoundError:
    long_description = "Lumitec Poco CAN Protocol API - Python Implementation"

setup(
    name="lumitec-poco",
    version="1.0.0",
    author="Lumitec",
    author_email="support@lumitec.com",
    description="Python library for Lumitec Poco lighting system CAN protocol",
    long_description=long_description,
    long_description_content_type="text/markdown",
    url="https://github.com/lumitec/poco-api",
    packages=find_packages(),
    py_modules=["lumitec_poco"],
    classifiers=[
        "Development Status :: 5 - Production/Stable",
        "Intended Audience :: Developers",
        "Topic :: System :: Hardware :: Hardware Drivers",
        "Topic :: Software Development :: Libraries :: Python Modules",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3",
        "Programming Language :: Python :: 3.7",
        "Programming Language :: Python :: 3.8",
        "Programming Language :: Python :: 3.9",
        "Programming Language :: Python :: 3.10",
        "Programming Language :: Python :: 3.11",
        "Operating System :: OS Independent",
    ],
    python_requires=">=3.7",
    install_requires=[
        # No required dependencies - works with stdlib only
    ],
    extras_require={
        "can": ["python-can>=4.0.0"],
        "socketcan": [],  # Built into Linux
        "pcan": ["pypcan"],
        "dev": ["pytest", "pytest-cov", "black", "flake8"],
    },
    entry_points={
        "console_scripts": [
            "poco-example=example_usage:main",
            "poco-test=test_poco_api:main",
        ],
    },
    include_package_data=True,
    keywords="lumitec poco can nmea2000 lighting marine automotive",
    project_urls={
        "Bug Reports": "https://github.com/lumitec/poco-api/issues",
        "Source": "https://github.com/lumitec/poco-api",
        "Documentation": "https://lumitec.com/poco-api-docs",
    },
)
