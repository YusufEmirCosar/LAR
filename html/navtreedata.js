/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "LAR Packet Monitor", "index.html", [
    [ "LAR Packet Monitor documentation", "index.html", "index" ],
    [ "Architecture", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html", [
      [ "Architectural style", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md5", null ],
      [ "Layer rules", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md6", null ],
      [ "Core class UML", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md7", null ],
      [ "Domain and mapping model", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md8", null ],
      [ "Runtime component graph", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md9", null ],
      [ "Composition roots", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md10", null ],
      [ "CMake target graph", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md11", null ],
      [ "Stable boundaries and invariants", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md12", null ],
      [ "Safe extension points", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md13", null ]
    ] ],
    [ "Component and API reference", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html", [
      [ "Domain", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md15", [
        [ "Packet-monitor values", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md16", null ],
        [ "DLZ teaching domain", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md17", null ]
      ] ],
      [ "Application values and policy", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md18", null ],
      [ "Application ports", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md19", [
        [ "Runtime boundary", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md20", null ]
      ] ],
      [ "Application services and coordinators", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md21", null ],
      [ "Infrastructure adapters", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md22", [
        [ "Mapping and network", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md23", null ],
        [ "Session and timing", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md24", null ],
        [ "Runtime workers", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md25", null ]
      ] ],
      [ "Presentation shell", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md26", null ],
      [ "Map subsystem", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md27", null ],
      [ "LAR viewport subsystem", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md28", null ],
      [ "Plane visualization subsystem", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md29", null ],
      [ "DLZ presentation", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md30", null ],
      [ "Build-time map compiler", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md31", null ]
    ] ],
    [ "Concurrency model", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html", [
      [ "Thread topology", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md33", null ],
      [ "Ownership UML", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md34", null ],
      [ "Command transport", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md35", null ],
      [ "Source generations", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md36", null ],
      [ "Visual publication rates", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md37", null ],
      [ "Recording batching and bounds", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md38", null ],
      [ "Drain barrier", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md39", null ],
      [ "Immutable persistence handoff", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md40", null ],
      [ "Asynchronous viewport preparation", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md41", null ],
      [ "Shutdown order", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md42", null ],
      [ "Concurrency invariants", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md43", null ]
    ] ],
    [ "Runtime data flows", "md_docs_2_d_a_t_a___f_l_o_w_s.html", [
      [ "Mapping installation and online activation", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md45", null ],
      [ "Datagram acceptance and presentation", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md46", null ],
      [ "Recording append and backpressure", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md47", null ],
      [ "Snapshot barrier", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md48", null ],
      [ "Offline source transition and playback", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md49", null ],
      [ "Source epoch rejection", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md50", null ],
      [ "Build-time and runtime map flow", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md51", null ],
      [ "LAR zone preparation and drawing", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md52", null ],
      [ "DLZ input arbitration", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md53", null ]
    ] ],
    [ "Dynamic Launch Zone teaching model", "md_docs_2_d_l_z___m_o_d_e_l.html", [
      [ "Isolation boundary", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md55", null ],
      [ "Domain values", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md56", null ],
      [ "Scenario adapter", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md57", null ],
      [ "Geometry", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md58", null ],
      [ "Exact toy equations", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md59", null ],
      [ "Supported ordered domain", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md60", null ],
      [ "Input modes", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md61", null ],
      [ "Presentation state", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md62", [
        [ "Range filter", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md63", null ],
        [ "Scale hysteresis", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md64", null ],
        [ "Shoot cue hysteresis", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md65", null ],
        [ "Rendering", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md66", null ]
      ] ],
      [ "Verification", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md67", null ]
    ] ],
    [ "LAR1 session format", "md_docs_2_l_a_r1___f_o_r_m_a_t.html", [
      [ "File layout", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md69", null ],
      [ "Record layout", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md70", null ],
      [ "Resource limits", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md71", null ],
      [ "Writer contract", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md72", null ],
      [ "Immutable snapshots", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md73", null ],
      [ "Reader validation pass", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md74", null ],
      [ "Playback interpretation", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md75", null ],
      [ "Compatibility and evolution", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md76", null ],
      [ "Worked hexadecimal example", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md77", null ]
    ] ],
    [ "Packet protocol and units", "md_docs_2_p_r_o_t_o_c_o_l___u_n_i_t_s.html", [
      [ "Mapping entry", "md_docs_2_p_r_o_t_o_c_o_l___u_n_i_t_s.html#autotoc_md79", null ],
      [ "Canonical fields", "md_docs_2_p_r_o_t_o_c_o_l___u_n_i_t_s.html#autotoc_md80", null ],
      [ "Position and angle conventions", "md_docs_2_p_r_o_t_o_c_o_l___u_n_i_t_s.html#autotoc_md81", null ],
      [ "Availability semantics", "md_docs_2_p_r_o_t_o_c_o_l___u_n_i_t_s.html#autotoc_md82", null ],
      [ "Decode transaction", "md_docs_2_p_r_o_t_o_c_o_l___u_n_i_t_s.html#autotoc_md83", null ],
      [ "Supplied mappings", "md_docs_2_p_r_o_t_o_c_o_l___u_n_i_t_s.html#autotoc_md84", null ]
    ] ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", "namespacemembers_dup" ],
        [ "Functions", "namespacemembers_func.html", "namespacemembers_func" ],
        [ "Variables", "namespacemembers_vars.html", "namespacemembers_vars" ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ]
      ] ]
    ] ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ],
        [ "Enumerator", "functions_eval.html", null ],
        [ "Related Symbols", "functions_rela.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", null ],
        [ "Functions", "globals_func.html", null ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Enumerations", "globals_enum.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ]
  ] ]
];

var NAVTREEINDEX =
[
"annotated.html",
"class_earth_lar_view.html#a25b6ff33143a8b9605b26e9de658b266",
"class_i_playback_runtime.html#ad3ed6047071aa7ffc0194091c8cc347f",
"class_lar_zone_gpu_layer.html#a234ab086fd52fcbc63abeba69cb48ea7",
"class_persistence_runtime_worker.html#a2908a5379ba1a7e5642f0c49e71d7ace",
"class_playback_metrics.html#a2afa7169a7c3c8963797f2d3dc1a81f8",
"class_recording_service.html#a92e22198dc632de37f055bfb0ab14889",
"class_threaded_application_runtime.html#abca0fd6208ac06f5097335c7e588e219",
"classdlz_1_1presentation_1_1_hud_view.html#a2de74cf8d1ad6fbbc5a7bdd8f03af6b5",
"classlar_1_1map_1_1_earth_map_widget.html#a463b43a276cdb05dd79a17b94a3b1def",
"dir_844f6ab5c2ae79211596f3dcdd81adb7.html",
"json__mapping__repository_8h.html",
"namespace_test_sender_scenarios_1_1anonymous__namespace_02scenarios_8cpp_03.html#a050fd9c5789148b32ab0f909aef57c26",
"namespaceanonymous__namespace_02plane__scene__renderer_8cpp_03.html#a905594cd28b7bb5defe7c6c45dd5faae",
"namespacelar_1_1map_1_1tool.html#a1da6af4139e2a8138d6a53a3e4049009a5b9e324ce43a64f4976de212c9d3b75d",
"plane__surface__state_8h.html#a971ccf165adc84ef7404b980a332fc35",
"struct_dted_cell_read_result.html#a758e046c136129e7d00daa53aa95edbf",
"struct_recording_save_result.html#abf7c427e32060011eb1a51f8815d3c39",
"structdlz_1_1_solution.html#a0100cf7c6f3b36cfa4166965e0b7e82e"
];

const SYNCONMSG = 'click to disable panel synchronization';
const SYNCOFFMSG = 'click to enable panel synchronization';
const LISTOFALLMEMBERS = 'List of all members';