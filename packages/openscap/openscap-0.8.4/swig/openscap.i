/*
 * Open Vulnerability and Assessment Language
 * (http://oval.mitre.org/)
 */

/*
 * Copyright 2009 Red Hat Inc., Durham, North Carolina.
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      Maros Barabas   <mbarabas@redhat.com>
 *
 */


/* Convert a tuple element to a C array */
/* SWIG can't understand __attribute__(x), so we make it go away */
#define __attribute__(x)

#if defined(SWIGPYTHON)

/* Definitions for PYTHON */

%exception xccdf_policy_evaluate {
    Py_BEGIN_ALLOW_THREADS
    $action
    Py_END_ALLOW_THREADS
}

%typemap(in) time_t
{
    if (PyLong_Check($input))
        $1 = (time_t) PyLong_AsLong($input);
    else if (PyInt_Check($input))
        $1 = (time_t) PyInt_AsLong($input);
    else if (PyFloat_Check($input))
        $1 = (time_t) PyFloat_AsDouble($input);
    else {
        PyErr_SetString(PyExc_TypeError,"Expected a large number");
        return NULL;
    }
}

%typemap(out) time_t
{
    $result = PyLong_FromLong((long)$1);
}

%typemap(in) void * {
    $result = SWIG_ConvertPtr($input,%as_voidptrptr(&$1), 0, $disown);
    if (!SWIG_IsOK($result)) {
        %argument_fail($result, "$type", $symname, $argnum);
    }
}


%module argv

%typemap(in) char ** {
  if (PyList_Check($input)) {
    int size = PyList_Size($input);
    int i = 0;
    $1 = (char **) malloc((size+1)*sizeof(char *));
    for (i = 0; i < size; i++) {
      PyObject *o = PyList_GetItem($input,i);
      /*if (PyString_Check(o))*/
        $1[i] = PyString_AsString(PyList_GetItem($input,i));
      /*else {
        PyErr_SetString(PyExc_TypeError,"list must contain strings");
        free($1);
        return NULL;
      }*/
    }
    $1[i] = 0;
  } else {
    PyErr_SetString(PyExc_TypeError,"not a list");
    return NULL;
  }
}

%typemap(freearg) char ** {
  free((char *) $1);
}

%typemap(in) struct cpe_name ** {

    int i; 
    int a_size = 0;
    int res_o = 0;
    a_size = (int) PySequence_Length($input);

    if (!PySequence_Check($input)) {
        PyErr_SetString(PyExc_ValueError,"Expected a sequence");
        return NULL;
    }
    if (a_size <= 0) {
        PyErr_SetString(PyExc_ValueError,"Expected not empty sequence");
        return NULL;
    }

    $1 = (struct cpe_name **) malloc(a_size*sizeof(struct cpe_name *));
    for (i = 0; i < a_size; i++) {
        void *arg = 0;
        PyObject *obj = PySequence_GetItem($input,i);
        if (obj == NULL) {
            SWIG_exception_fail(SWIG_ArgError(res_o), "in argument '" "cpe_name" "' substitution '" "', can't access sequence");
            return NULL; /*5956*/
        }
        res_o = SWIG_ConvertPtr(obj, &arg, SWIGTYPE_p_cpe_name, 0 );
        if (!SWIG_IsOK(res_o)) {
            SWIG_exception_fail(SWIG_ArgError(res_o), "in argument '" "cpe_name" "' substitution invalid types");
            return NULL;
        }
        $1[i] = (struct cpe_name *) arg;
    }
}
%typemap(in) struct oval_syschar_model ** {

    int i; 
    int a_size = 0;
    int res_o = 0;
    a_size = (int) PySequence_Length($input);

    if (!PySequence_Check($input)) {
        PyErr_SetString(PyExc_ValueError,"Expected a sequence");
        return NULL;
    }
    if (a_size <= 0) {
        PyErr_SetString(PyExc_ValueError,"Expected not empty sequence");
        return NULL;
    }

    $1 = (struct oval_syschar_model **) malloc(a_size*sizeof(struct oval_syschar_model *));
    for (i = 0; i < a_size; i++) {
        void *arg = 0;
        PyObject *obj = PySequence_GetItem($input,i);
        if (obj == NULL) {
            SWIG_exception_fail(SWIG_ArgError(res_o), "in argument '" "oval_syschar_model" "' substitution '" "', can't access sequence");
            return NULL;
        }
        res_o = SWIG_ConvertPtr(obj, &arg, SWIGTYPE_p_oval_syschar_model, 0 );
        if (!SWIG_IsOK(res_o)) {
            SWIG_exception_fail(SWIG_ArgError(res_o), "in argument '" "oval_syschar_model" "' substitution invalid types");
            return NULL;
        }
        $1[i] = (struct oval_syschar_model *) arg;
    }
}
#elif defined(SWIGPERL)
/* Definitions for PERL */
#endif

%module openscap
%{
 #include "../src/common/public/oscap.h"
 #include "../src/common/public/error.h"
 #include "../src/common/public/debug.h"
 #include "../src/common/public/text.h"
 #include "../src/common/public/reporter.h"
 #include "../src/common/public/reference.h"
%}

%include "../src/common/public/oscap.h"
%include "../src/common/public/error.h"
%include "../src/common/public/debug.h"
%include "../src/common/public/text.h"
%include "../src/common/public/reporter.h"
%include "../src/common/public/reference.h"

#ifdef WANT_CCE
%module openscap
%{
 #include "../src/CCE/public/cce.h"
%}
%include "../src/CCE/public/cce.h"
#endif


#ifdef WANT_CPE
%module openscap
%{
 #include "../src/CPE/public/cpeuri.h"
 #include "../src/CPE/public/cpedict.h"
 #include "../src/CPE/public/cpelang.h"
%}
%include "../src/CPE/public/cpeuri.h"
%include "../src/CPE/public/cpedict.h"
%include "../src/CPE/public/cpelang.h"
#endif


#ifdef WANT_CVE
%module openscap
%{
 #include "../src/CVE/public/cve.h"
%}
%include "../src/CVE/public/cve.h"
#endif


#ifdef WANT_CVSS
%module openscap
%{
 #include "../src/CVSS/public/cvss.h"
%}
%include "../src/CVSS/public/cvss.h"
#endif

#ifdef WANT_XCCDF
%module openscap
%{
 #include "../src/XCCDF/public/xccdf.h"
 #include "../src/XCCDF_POLICY/public/xccdf_policy.h"
%}
%include "../src/XCCDF/public/xccdf.h"
%include "../src/XCCDF_POLICY/public/xccdf_policy.h"
#endif


#ifdef WANT_OVAL
%module openscap
%{
 #include "../src/OVAL/public/oval_agent_api.h"
 #include "../src/OVAL/public/oval_definitions.h"
 #include "../src/OVAL/public/oval_system_characteristics.h"
 #include "../src/OVAL/public/oval_results.h"
 #include "../src/OVAL/public/oval_types.h"
 #include "../src/OVAL/public/oval_variables.h"
 #include "../src/OVAL/public/oval_probe.h"
 #include "../src/OVAL/public/oval_probe_handler.h"
 #include "../src/OVAL/public/oval_probe_session.h"
%}
%include "../src/OVAL/public/oval_agent_api.h"
%include "../src/OVAL/public/oval_definitions.h"
%include "../src/OVAL/public/oval_system_characteristics.h"
%include "../src/OVAL/public/oval_results.h"
%include "../src/OVAL/public/oval_types.h"
%include "../src/OVAL/public/oval_variables.h"
%include "../src/OVAL/public/oval_probe.h"
%include "../src/OVAL/public/oval_probe_handler.h"
%include "../src/OVAL/public/oval_probe_session.h"
#endif

#ifdef WANT_OVAL
#ifdef WANT_XCCDF
%module openscap
%{
 #include "../src/OVAL/public/oval_agent_xccdf_api.h"
%}
%include "../src/OVAL/public/oval_agent_xccdf_api.h"
#endif

#endif

#ifdef WANT_SCE
%module openscap
%{
 #include "../src/SCE/public/sce_engine_api.h"
%}
%include "../src/SCE/public/sce_engine_api.h"
#endif

/*%typemap(in) value[ANY] {

    int i;
    PyErr_SetString(PyExc_ValueError,"Expected a sequence");
    return NULL;
    if (!PySequence_Check($input)) {
        PyErr_SetString(PyExc_ValueError,"Expected a sequence");
        return NULL;
    }
    if (PySequence_Length($input) != $1_dim0) {
            PyErr_SetString(PyExc_ValueError,"Size mismatch. Expected $1_dim0 elements");
            return NULL;
    }
    $1 = (void **) malloc($1_dim0*sizeof( Py_TYPE(PySequence_GetItem($input, 0)) ));

    for (i = 0; i < $1_dim0; i++) {
        PyObject *o = PySequence_GetItem($input,i);
        if (Py_TYPE(PySequence_GetItem($input,i)) != Py_TYPE(PySequence_GetItem($input,0)) {
                PyErr_SetString(PyExc_ValueError,"Type mismatch");
                free($1);
                return NULL;
        }
        $1[i] = (PyTYPE(PySequence_GetItem($input, i))) o;
    }

}

*/

#if defined(SWIGPYTHON)

#ifdef WANT_XCCDF

%{
struct internal_usr {
    PyObject *func;
    PyObject *usr;
};

static int xccdf_policy_model_callback_wrapper(struct xccdf_policy *policy, char *href, char *id, struct xccdf_value_binding_iterator *it, void *usr)
{
    PyGILState_STATE state;
    PyObject *arglist;
    PyObject *py_policy;
    PyObject *py_bindings;
    PyObject *func, *usrdata;
    struct internal_usr *data;
    PyObject *result;
    double    dres = 0;

    state = PyGILState_Ensure();
    py_policy = SWIG_NewPointerObj(policy, SWIGTYPE_p_xccdf_policy, 1);
    py_bindings = SWIG_NewPointerObj(it, SWIGTYPE_p_xccdf_value_binding_iterator, 1);
    data = (struct internal_usr *)usr;
    func = data->func;
    usrdata = data->usr;
    arglist = Py_BuildValue("OssOO", py_policy, href, id, py_bindings, usrdata);
    if (!PyCallable_Check(func)) {
      PyGILState_Release(state);
      return false;
    }
    result = PyEval_CallObject(func,arglist);
    if (result == NULL) {
        if (PyErr_Occurred() != NULL)
            PyErr_PrintEx(0);
        PyErr_Print();
        Py_DECREF(arglist);
        Py_XDECREF(result);
        PyGILState_Release(state);
        return false;
    }
    Py_DECREF(arglist);
    dres = PyInt_AsLong(result);
    Py_XDECREF(result);
    PyGILState_Release(state);
    return dres;
}

int output_callback_wrapper(const struct oscap_reporter_message *msg, void *arg)
{
    PyGILState_STATE state;
    PyObject *py_msg;
    PyObject *arglist;
    PyObject *func, *usrdata;
    struct internal_usr *data;
    PyObject *result;
    double    dres = 0;

    state = PyGILState_Ensure();
    py_msg = SWIG_NewPointerObj(msg, SWIGTYPE_p_oscap_reporter_message, 1);
    data = (struct internal_usr *) arg;
    func = data->func;
    usrdata = data->usr;
    arglist = Py_BuildValue("OO", py_msg, usrdata);
    if (!PyCallable_Check(func)) {
      PyGILState_Release(state);
      return 1;
    }
    result = PyEval_CallObject(func,arglist);
    if (result == NULL) {
        if (PyErr_Occurred() != NULL)
            PyErr_PrintEx(0);
        PyErr_Print();
        Py_DECREF(arglist);
        Py_XDECREF(result);
        PyGILState_Release(state);
        return -1;
    }
    Py_DECREF(arglist);
    dres = PyInt_AsLong(result);
    Py_XDECREF(result);
    PyGILState_Release(state);
    return dres;
}

char * sub_callback_wrapper(xccdf_subst_type_t type, const char *id, void *arg)
{
    PyGILState_STATE state;
    PyObject *arglist;
    PyObject *func, *usrdata;
    struct internal_usr *data;
    PyObject *result;
    char *dres = NULL;

    state = PyGILState_Ensure();
    data = (struct internal_usr *) arg;
    func = data->func;
    usrdata = data->usr;

    arglist = Py_BuildValue("isO", type, id, usrdata);
    if (!PyCallable_Check(func)) {
      PyGILState_Release(state);
      return 1;
    }
    result = PyEval_CallObject(func, arglist);
    if (result == NULL) {
        if (PyErr_Occurred() != NULL)
            PyErr_PrintEx(0);
        PyErr_Print();
        Py_DECREF(arglist);
        Py_XDECREF(result);
        PyGILState_Release(state);
        return NULL;
    }
    Py_DECREF(arglist);
    dres = PyString_AsString(result);
    if (dres == NULL) {
        if (PyErr_Occurred() != NULL)
            PyErr_PrintEx(0);
        PyErr_Print();
        Py_DECREF(arglist);
        Py_XDECREF(result);
        PyGILState_Release(state);
        return NULL;
    }
    Py_XDECREF(result);
    PyGILState_Release(state);
    return strdup(dres);
}

%}

%inline %{

bool xccdf_policy_model_register_engine_callback_py(struct xccdf_policy_model *model, char *sys, PyObject *func, PyObject *usr) {
    struct internal_usr *new_usrdata;
    PyEval_InitThreads();
    Py_INCREF(func);
    Py_INCREF(usr);
    new_usrdata = malloc(sizeof(struct internal_usr));
    if (new_usrdata == NULL) return false;

    new_usrdata->func = func;
    new_usrdata->usr = usr;
  
    return xccdf_policy_model_register_engine_callback(model, sys, xccdf_policy_model_callback_wrapper, (void *)new_usrdata);
}

bool xccdf_policy_model_register_output_callback_py(struct xccdf_policy_model *model, PyObject *func, PyObject *usr) {
    struct internal_usr *new_usrdata;
    PyEval_InitThreads();
    Py_INCREF(func);
    Py_INCREF(usr);
    new_usrdata = malloc(sizeof(struct internal_usr));
    if (new_usrdata == NULL) return false;

    new_usrdata->func = func;
    new_usrdata->usr = usr;
  
    return xccdf_policy_model_register_output_callback(model, output_callback_wrapper, (void *)new_usrdata);
}

bool xccdf_policy_model_register_start_callback_py(struct xccdf_policy_model *model, PyObject *func, PyObject *usr) {
    struct internal_usr *new_usrdata;
    PyEval_InitThreads();
    Py_INCREF(func);
    Py_INCREF(usr);
    new_usrdata = malloc(sizeof(struct internal_usr));
    if (new_usrdata == NULL) return false;

    new_usrdata->func = func;
    new_usrdata->usr = usr;
  
    return xccdf_policy_model_register_start_callback(model, output_callback_wrapper, (void *)new_usrdata);
}

int oval_agent_eval_system_py(oval_agent_session_t * asess, PyObject * func, PyObject *usr) {
    struct internal_usr *new_usrdata;
    PyEval_InitThreads();
    Py_INCREF(func);
    Py_INCREF(usr);
    new_usrdata = malloc(sizeof(struct internal_usr));
    if (new_usrdata == NULL) return false;

    new_usrdata->func = func;
    new_usrdata->usr = usr;
  
    return oval_agent_eval_system(asess, output_callback_wrapper, (void *) new_usrdata);
}

bool oscap_validate_document_py(const char *xmlfile, oscap_document_type_t doctype, const char *version, PyObject *func, PyObject *usr) {
    struct internal_usr *new_usrdata;
    PyEval_InitThreads();
    Py_INCREF(func);
    Py_INCREF(usr);
    new_usrdata = malloc(sizeof(struct internal_usr));
    if (new_usrdata == NULL) return false;

    new_usrdata->func = func;
    new_usrdata->usr = usr;
  
    return oscap_validate_document(xmlfile, doctype, version, output_callback_wrapper, (void *)new_usrdata);
}

char * oscap_text_xccdf_substitute_py(const char *text, PyObject *func, PyObject *usr) {
    struct internal_usr *new_usrdata;
    PyEval_InitThreads();
    Py_INCREF(func);
    Py_INCREF(usr);
    new_usrdata = malloc(sizeof(struct internal_usr));
    if (new_usrdata == NULL) return false;

    new_usrdata->func = func;
    new_usrdata->usr = usr;
  
    return oscap_text_xccdf_substitute(text, sub_callback_wrapper, (void *)new_usrdata);
}

%}
#endif
#endif
