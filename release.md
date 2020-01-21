# Release process

The release process is as follows:

 - Update the version number in `setup.py`.
 - Update the version number in `src/main.cpp`.
 - Run `release.sh`. This will build the Python wheel in a manylinux docker
   container and stick it in `dist/*.whl`.
 - Install the wheel locally.
 - Test using `nosetests dqcsim_openql_mapper`
 - Push the wheel to PyPI with twine:

```
python3 -m pip install --user --upgrade twine
python3 -m twine upload dist/*
```

## Auditwheel notes

We're doing some pretty shifty stuff in the release script to have PyPI accept
the fact that we're dynamically linking against `libdqcsim.so`.

According to PEP-513, all wheels must be self-contained; that is, they should
include all but some very basic, very ancient C library stuff. The problem with
this, aside from the obvious (large file sizes, load times, missing security
patches because everyone would have to redistribute when a vulnerability is
found, ...), is two-fold:

 - If a bug is found in DQCsim, all plugins need to be rebuilt to link against
   the new version for the bug to be fixed.
 - If DQCsim's IPC protocol needs to be updated, old plugins would stop working
   with arcane errors.

Neither of these are acceptable within the context of a research problems.
Note that we're not the only ones having issues with this stuff (pyarrow is
another, rather big player).

So what we're doing is patching PyPa's packaging module that pulls in
dependencies (`auditwheel`) to whitelist `libdqcsim.so`. Basically, lying. It
ain't pretty, but that's the way it is right now.
