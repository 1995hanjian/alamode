.. raw:: html

    <script type="text/javascript"><!--
      function gen_mail_to_link(name, lhs,rhs)
      {
       document.write("<br>");
       document.write(name + " (<A HREF=\"mailto");
       document.write(":" + lhs + "@");
       document.write(rhs + "\">" + lhs + "@" + rhs + "<\/A>)"); } 
       // --> </SCRIPT>


About
=====

What is ALAMODE?
-----------------

**ALAMODE** is an open source software designed for analyzing lattice anharmonicity and lattice thermal conductivity of solids. By using an external DFT package such as VASP and Quantum ESPRESSO, you can extract harmonic and anharmonic force constants straightforwardly with ALAMODE. Using the calculated anharmonic force constants, you can also estimate lattice thermal conductivity, phonon linewidth, and other anharmonic phonon properties from first principles.

Features
--------

General
^^^^^^^

* Extraction of harmonic and anharmonic force constants based on the supercell approach
* Applicable to any crystal structures and low-dimensional systems
* Accurate treatment of translational and rotational invariance
* Interface to VASP, Quantum-ESPRESSO, and xTAPP codes
* Mainly written in C++, parallelized with MPI+OpenMP

Harmonic properties
^^^^^^^^^^^^^^^^^^^
* Phonon dispersion
* Phonon DOS, atom-projected phonon DOS
* Two-phonon DOS
* Vibrational thermodynamic functions (heat capacity, entropy, free energy)
* Mean-square displacement
* Animation and visualization of phonon modes (requires VMD or XCrysDen)
* 3-phonon scattering phase space
* Phonon-isotope scattering rate
* Participation ratio for analyzing localization of phonon modes

Anharmonic properties
^^^^^^^^^^^^^^^^^^^^^

.. |umulaut_u|    unicode:: U+00FC

* Gr\ |umulaut_u|\ neisen parameter via cubic force constants
* Lattice thermal conductivity by BTE-RTA
* Cumulative thermal conductivity
* Phonon linewidth due to 3-phonon interactions
* Phonon frequency shift due to 3- and 4-phonon interactions
* Temperature-dependent effective potential method


Links
-----

* Download page  : http://sourceforge.net/projects/alamode 
* Documentation  : http://alamode.readthedocs.io (this page)
* Git repository : http://github.com/ttadano/alamode


License
-------

.. |copy|   unicode:: U+000A9 

Copyright |copy| 2014, 2015, 2016 Terumasa Tadano

This software is distributed under the MIT license.
See the LICENSE.txt file for license rights and limitations. 


How to Cite ALAMODE
-------------------

Please cite the following article when you use ALAMODE:

  T\. Tadano, Y. Gohda, and S. Tsuneyuki, J. Phys.: Condens. Matter **26**\ , 225402 (2014) [Link_].

.. _Link : http://iopscience.iop.org/0953-8984/26/22/225402/


Acknowledgement
---------------

This project was supported by a Grant-in-Aid for Scientific Research on Innovative Areas 
'Materials Design through Computics: Complex Correlation and Non-Equilibrium Dynamics'.
(http://computics-material.jp)


Author & Contact
----------------

.. raw:: html

    <script>gen_mail_to_link('Terumasa TADANO', 'terumasa.tadano','gmail.com')</script>

Department of Applied Physics, 
The University of Tokyo, Japan

If you have any questions, suggestions, and problems regarding ALAMODE, please feel free to contact the author.

