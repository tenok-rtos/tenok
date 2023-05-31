#!/usr/bin/env python

from distutils.core import setup

setup(name='rtplot',
      version='1.0',
      description='Tenok Real-time Data Plotting Tool',
      author='Shengwen Cheng',
      author_email='shengwen1997.tw@gmail.com',
      url='https://github.com/shengwen-tw/tenok',
      package_dir={'': 'src'},
      packages = ['rtplot'],
      scripts=['scripts/rtplot']
     )
