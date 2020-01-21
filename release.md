# Release process

The release process is as follows:

 - Update the version number in `setup.py`.
 - Update the version number in `src/main.cpp`.
 - Run `release.sh`. This will build the Python wheel in a manylinux docker
   container and stick it in `dist/*.whl`.
 - Install the wheel locally and test it with `setup.py test`.
 - Push the wheel to PyPI with twine:

```
python3 -m pip install --user --upgrade twine
python3 -m twine upload dist/*
```
