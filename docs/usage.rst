Using *libprofit*
=================

From the command-line
---------------------

*libprofit* ships with a command-line utility ``profit-cli``
that reads the model and profile parameters from the command-line
and generates the corresponding image.
It supports all the profiles supported by *libprofit*,
and can output the resulting image as text values, a binary stream,
or as a simple FITS file.

Run ``profit-cli -h`` for a full description on how to use it,
how to specify profiles and model parameters,
and how to control its output.

Programatically
---------------

As it name implies, *libprofit* also ships a shared library
exposing an API that can be used by any third-party application.
This section gives a brief overview on how to use this API.
For a full reference please refer to :doc:`api`.

.. default-domain:: c
.. highlight:: c

At the core of *libprofit* sits :type:`profit_model`.
This structure holds all the information needed to generate an image.
Different profiles (instances of :type:`profit_profile`)
are appended to the model, which is then evaluated.

The basic usage pattern then is as follows:

#. First obtain a model instance::

	 profit_model *model = profit_create_model();

#. Create a profile. For a list of supported names see :doc:`profiles`;
   if you want to support a new profile see :doc:`new_profile`::

	 profit_profile *sersic_profile = profit_create_profile(model, "sersic");

#. Customize your profile.
   An explicit cast must be performed on the :type:`profit_profile` to turn it
   into the specific profile type.
   By convention these sub-types are named after the profile they represent,
   like this::

	 profit_sersic_profile *sp = (profit_sersic_profile *)sersic_profile;
	 sp->xcen = 34.67;
	 sp->ycen = 9.23;
	 sp->axrat = 0.345;
	 [...]

#. Repeat the previous two steps for all profiles
   you want to include in your model.

#. Evaluate the model simply run::

	 profit_eval_model(model);

#. After running check if there are have been errors
   while generating the image.
   If no errors occurred you can safely access the data
   stored in :member:`profit_model.image`::

	 char *error = profit_get_error(model);
	 if( error ) {
	     printf("Oops! There was an error evaluating the model: %s", error);
	 }
	 else {
	    do_something_with_your_image(model->image);
	 }

#. Finally dispose of the model.
   This should **always** be called,
   regardless of whether the model was actually used or not,
   or whether its evaluation was successful or not::

	 profit_cleanup(model);

