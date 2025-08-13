# Makefile for Lumitec Poco Python API

# Python interpreter
PYTHON = python3

# Package name
PACKAGE = lumitec_poco

.PHONY: help test example clean install install-dev lint format docs

help:
	@echo "Available targets:"
	@echo "  test        - Run test suite"
	@echo "  example     - Run example usage"
	@echo "  integration - Run integration examples"
	@echo "  install     - Install package"
	@echo "  install-dev - Install in development mode with dev dependencies"
	@echo "  lint        - Run linting (requires flake8)"
	@echo "  format      - Format code (requires black)"
	@echo "  clean       - Clean up generated files"
	@echo "  package     - Create distribution package"

test:
	$(PYTHON) test_poco_api.py

example:
	$(PYTHON) example_usage.py

integration:
	$(PYTHON) integration_examples.py

install:
	$(PYTHON) -m pip install .

install-dev:
	$(PYTHON) -m pip install -e ".[dev]"

lint:
	flake8 $(PACKAGE).py example_usage.py test_poco_api.py integration_examples.py

format:
	black $(PACKAGE).py example_usage.py test_poco_api.py integration_examples.py

clean:
	rm -rf build/
	rm -rf dist/
	rm -rf *.egg-info/
	find . -type d -name __pycache__ -exec rm -rf {} +
	find . -type f -name "*.pyc" -delete

package: clean
	$(PYTHON) setup.py sdist bdist_wheel

# Quick test for basic functionality
quick-test:
	@echo "Quick functionality test..."
	@$(PYTHON) -c "from lumitec_poco import *; print('✓ Import successful')"
	@$(PYTHON) -c "from lumitec_poco import create_simple_action, PocoActionID; f=create_simple_action(0x0E,0x10,PocoActionID.ON,1); print(f'✓ Message creation: {f.data.hex().upper()}')"
	@$(PYTHON) -c "from lumitec_poco import LumitecPocoAPI; print(f'✓ Version: {LumitecPocoAPI.get_version()}')"

# Performance test
perf-test:
	@echo "Performance test..."
	@$(PYTHON) -c "import time; from lumitec_poco import create_simple_action, PocoActionID; start=time.time(); [create_simple_action(0x0E,0x10,PocoActionID.ON,1) for _ in range(10000)]; print(f'✓ Created 10k messages in {time.time()-start:.3f}s')"

# All tests
all-tests: quick-test test perf-test
	@echo "✓ All tests completed successfully!"
