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
    [ "LAR Packet Monitor", "md__r_e_a_d_m_e.html", [
      [ "Architecture", "md__r_e_a_d_m_e.html#autotoc_md15", null ],
      [ "Build", "md__r_e_a_d_m_e.html#autotoc_md16", [
        [ "Quality presets and gates", "md__r_e_a_d_m_e.html#autotoc_md17", null ]
      ] ],
      [ "Online Mode", "md__r_e_a_d_m_e.html#autotoc_md18", null ],
      [ "Test Sender", "md__r_e_a_d_m_e.html#autotoc_md19", null ],
      [ "Offline Mode", "md__r_e_a_d_m_e.html#autotoc_md20", null ],
      [ "<span class=\"tt\">.lar</span> Format", "md__r_e_a_d_m_e.html#autotoc_md21", null ],
      [ "Current Values", "md__r_e_a_d_m_e.html#autotoc_md22", null ],
      [ "LAR Display", "md__r_e_a_d_m_e.html#autotoc_md23", null ],
      [ "Plane visualization", "md__r_e_a_d_m_e.html#autotoc_md24", null ],
      [ "Dynamic Launch Zone (DLZ)", "md__r_e_a_d_m_e.html#autotoc_md25", null ]
    ] ],
    [ "LAR Packet Monitor project specification", "md__project_specification.html", [
      [ "Purpose and status", "md__project_specification.html#autotoc_md27", null ],
      [ "Functional requirements", "md__project_specification.html#autotoc_md28", [
        [ "Input and online operation", "md__project_specification.html#autotoc_md29", null ],
        [ "Recording and playback", "md__project_specification.html#autotoc_md30", null ],
        [ "Visualization", "md__project_specification.html#autotoc_md31", null ],
        [ "Map, terrain, and Plane assets", "md__project_specification.html#autotoc_md32", null ],
        [ "User-selected Plane models", "md__project_specification.html#autotoc_md33", null ]
      ] ],
      [ "Architecture and lifecycle requirements", "md__project_specification.html#autotoc_md34", null ],
      [ "Dependency and packaging requirements", "md__project_specification.html#autotoc_md35", null ],
      [ "Security and operational constraints", "md__project_specification.html#autotoc_md36", null ],
      [ "Acceptance criteria", "md__project_specification.html#autotoc_md37", null ]
    ] ],
    [ "Architecture", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html", [
      [ "Architectural style", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md39", null ],
      [ "Layer rules", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md40", null ],
      [ "Core class UML", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md41", null ],
      [ "Domain and mapping model", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md42", null ],
      [ "Runtime component graph", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md43", null ],
      [ "Composition roots", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md44", null ],
      [ "CMake target graph", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md45", null ],
      [ "Stable boundaries and invariants", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md46", null ],
      [ "Safe extension points", "md_docs_2_a_r_c_h_i_t_e_c_t_u_r_e.html#autotoc_md47", null ]
    ] ],
    [ "Component and API reference", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html", [
      [ "Domain", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md49", [
        [ "Packet-monitor values", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md50", null ],
        [ "DLZ teaching domain", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md51", null ]
      ] ],
      [ "Application values and policy", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md52", null ],
      [ "Application ports", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md53", [
        [ "Runtime boundary", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md54", null ]
      ] ],
      [ "Application services and coordinators", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md55", null ],
      [ "Infrastructure adapters", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md56", [
        [ "Mapping and network", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md57", null ],
        [ "Session and timing", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md58", null ],
        [ "Runtime workers", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md59", null ]
      ] ],
      [ "Presentation shell", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md60", null ],
      [ "Map subsystem", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md61", null ],
      [ "LAR viewport subsystem", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md62", null ],
      [ "Plane visualization subsystem", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md63", [
        [ "Plane terrain pipeline", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md64", null ]
      ] ],
      [ "DLZ presentation", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md65", null ],
      [ "Build-time map compiler", "md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md66", null ]
    ] ],
    [ "Concurrency model", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html", [
      [ "Thread topology", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md68", null ],
      [ "Ownership UML", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md69", null ],
      [ "Command transport", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md70", null ],
      [ "Source generations", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md71", null ],
      [ "Visual publication rates", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md72", null ],
      [ "Recording batching and bounds", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md73", null ],
      [ "Drain barrier", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md74", null ],
      [ "Immutable persistence handoff", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md75", null ],
      [ "Asynchronous viewport preparation", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md76", null ],
      [ "Shutdown order", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md77", null ],
      [ "Concurrency invariants", "md_docs_2_c_o_n_c_u_r_r_e_n_c_y___m_o_d_e_l.html#autotoc_md78", null ]
    ] ],
    [ "Runtime data flows", "md_docs_2_d_a_t_a___f_l_o_w_s.html", [
      [ "Mapping installation and online activation", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md80", null ],
      [ "Datagram acceptance and presentation", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md81", null ],
      [ "Recording append and backpressure", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md82", null ],
      [ "Snapshot barrier", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md83", null ],
      [ "Offline source transition and playback", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md84", null ],
      [ "Source epoch rejection", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md85", null ],
      [ "Build-time and runtime map flow", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md86", null ],
      [ "Plane terrain request and installation", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md87", null ],
      [ "LAR zone preparation and drawing", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md88", null ],
      [ "DLZ input arbitration", "md_docs_2_d_a_t_a___f_l_o_w_s.html#autotoc_md89", null ]
    ] ],
    [ "Developer guide", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html", [
      [ "Prerequisites", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md91", null ],
      [ "Configure, build, and test", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md92", null ],
      [ "Repository checks", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md93", null ],
      [ "Read before changing structure", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md94", null ],
      [ "Add a packet field", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md95", null ],
      [ "Add an application use case", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md96", null ],
      [ "Add or replace an adapter", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md97", null ],
      [ "Add a viewport page", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md98", null ],
      [ "Change Plane visualization assets or rendering", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md99", null ],
      [ "Change the LAR1 format", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md100", null ],
      [ "Change the DLZ model", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md101", null ],
      [ "Map source and package changes", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md102", null ],
      [ "Doxygen style", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md103", null ],
      [ "Test selection", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md104", null ],
      [ "Safe-change checklist", "md_docs_2_d_e_v_e_l_o_p_e_r___g_u_i_d_e.html#autotoc_md105", null ]
    ] ],
    [ "Dynamic Launch Zone teaching model", "md_docs_2_d_l_z___m_o_d_e_l.html", [
      [ "Isolation boundary", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md107", null ],
      [ "Domain values", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md108", null ],
      [ "Scenario adapter", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md109", null ],
      [ "Geometry", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md110", null ],
      [ "Exact toy equations", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md111", null ],
      [ "Supported ordered domain", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md112", null ],
      [ "Input modes", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md113", null ],
      [ "Presentation state", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md114", [
        [ "Range filter", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md115", null ],
        [ "Scale hysteresis", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md116", null ],
        [ "Shoot cue hysteresis", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md117", null ],
        [ "Rendering", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md118", null ]
      ] ],
      [ "Verification", "md_docs_2_d_l_z___m_o_d_e_l.html#autotoc_md119", null ]
    ] ],
    [ "File reference", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html", [
      [ "Repository root", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md121", null ],
      [ "Automation and CMake modules", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md122", null ],
      [ "Assets and sample mappings", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md123", null ],
      [ "Domain source", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md124", null ],
      [ "Application source", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md125", [
        [ "Application ports and runtime protocol", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md126", null ]
      ] ],
      [ "Infrastructure source", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md127", null ],
      [ "Viewer shell and helpers", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md128", [
        [ "Dialogs, workflows, and panels", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md129", null ]
      ] ],
      [ "Map subsystem", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md130", null ],
      [ "Viewport subsystem", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md131", null ],
      [ "Plane visualization", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md132", null ],
      [ "DLZ HUD presentation", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md133", null ],
      [ "Sender source", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md134", null ],
      [ "Repository and map tools", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md135", null ],
      [ "Tests", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md136", [
        [ "Test support and fuzzing", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md137", null ]
      ] ],
      [ "Documentation files", "md_docs_2_f_i_l_e___r_e_f_e_r_e_n_c_e.html#autotoc_md138", null ]
    ] ],
    [ "LAR1 session format", "md_docs_2_l_a_r1___f_o_r_m_a_t.html", [
      [ "File layout", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md140", null ],
      [ "Record layout", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md141", null ],
      [ "Resource limits", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md142", null ],
      [ "Writer contract", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md143", null ],
      [ "Immutable snapshots", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md144", null ],
      [ "Reader validation pass", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md145", null ],
      [ "Playback interpretation", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md146", null ],
      [ "Compatibility and evolution", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md147", null ],
      [ "Worked hexadecimal example", "md_docs_2_l_a_r1___f_o_r_m_a_t.html#autotoc_md148", null ]
    ] ],
    [ "Packet protocol and units", "md_docs_2_p_r_o_t_o_c_o_l___u_n_i_t_s.html", [
      [ "Mapping entry", "md_docs_2_p_r_o_t_o_c_o_l___u_n_i_t_s.html#autotoc_md150", null ],
      [ "Canonical fields", "md_docs_2_p_r_o_t_o_c_o_l___u_n_i_t_s.html#autotoc_md151", null ],
      [ "Position and angle conventions", "md_docs_2_p_r_o_t_o_c_o_l___u_n_i_t_s.html#autotoc_md152", null ],
      [ "Availability semantics", "md_docs_2_p_r_o_t_o_c_o_l___u_n_i_t_s.html#autotoc_md153", null ],
      [ "Decode transaction", "md_docs_2_p_r_o_t_o_c_o_l___u_n_i_t_s.html#autotoc_md154", null ],
      [ "Supplied mappings", "md_docs_2_p_r_o_t_o_c_o_l___u_n_i_t_s.html#autotoc_md155", null ]
    ] ],
    [ "Quality gates", "md_docs_2_q_u_a_l_i_t_y___g_a_t_e_s.html", [
      [ "Current automated evidence", "md_docs_2_q_u_a_l_i_t_y___g_a_t_e_s.html#autotoc_md157", null ],
      [ "Configure and test presets", "md_docs_2_q_u_a_l_i_t_y___g_a_t_e_s.html#autotoc_md158", null ],
      [ "Repository and static gates", "md_docs_2_q_u_a_l_i_t_y___g_a_t_e_s.html#autotoc_md159", null ],
      [ "Deterministic CTest suite", "md_docs_2_q_u_a_l_i_t_y___g_a_t_e_s.html#autotoc_md160", null ],
      [ "Coverage", "md_docs_2_q_u_a_l_i_t_y___g_a_t_e_s.html#autotoc_md161", null ],
      [ "Sanitizers", "md_docs_2_q_u_a_l_i_t_y___g_a_t_e_s.html#autotoc_md162", null ],
      [ "Fuzzing", "md_docs_2_q_u_a_l_i_t_y___g_a_t_e_s.html#autotoc_md163", null ],
      [ "Mutation analysis", "md_docs_2_q_u_a_l_i_t_y___g_a_t_e_s.html#autotoc_md164", null ],
      [ "Performance evidence", "md_docs_2_q_u_a_l_i_t_y___g_a_t_e_s.html#autotoc_md165", null ],
      [ "Install and dependency evidence", "md_docs_2_q_u_a_l_i_t_y___g_a_t_e_s.html#autotoc_md166", null ],
      [ "GPU evidence", "md_docs_2_q_u_a_l_i_t_y___g_a_t_e_s.html#autotoc_md167", null ],
      [ "Operational soak", "md_docs_2_q_u_a_l_i_t_y___g_a_t_e_s.html#autotoc_md168", null ],
      [ "Release evidence checklist", "md_docs_2_q_u_a_l_i_t_y___g_a_t_e_s.html#autotoc_md169", null ]
    ] ],
    [ "SOLID compliance", "md_docs_2_s_o_l_i_d___c_o_m_p_l_i_a_n_c_e.html", [
      [ "Compliance summary", "md_docs_2_s_o_l_i_d___c_o_m_p_l_i_a_n_c_e.html#autotoc_md181", null ],
      [ "Single Responsibility Principle", "md_docs_2_s_o_l_i_d___c_o_m_p_l_i_a_n_c_e.html#autotoc_md182", null ],
      [ "Open/Closed Principle", "md_docs_2_s_o_l_i_d___c_o_m_p_l_i_a_n_c_e.html#autotoc_md183", null ],
      [ "Liskov Substitution Principle", "md_docs_2_s_o_l_i_d___c_o_m_p_l_i_a_n_c_e.html#autotoc_md184", null ],
      [ "Interface Segregation Principle", "md_docs_2_s_o_l_i_d___c_o_m_p_l_i_a_n_c_e.html#autotoc_md185", null ],
      [ "Dependency Inversion Principle", "md_docs_2_s_o_l_i_d___c_o_m_p_l_i_a_n_c_e.html#autotoc_md186", null ],
      [ "Mechanical protection", "md_docs_2_s_o_l_i_d___c_o_m_p_l_i_a_n_c_e.html#autotoc_md187", null ],
      [ "Review checklist for structural changes", "md_docs_2_s_o_l_i_d___c_o_m_p_l_i_a_n_c_e.html#autotoc_md188", null ]
    ] ],
    [ "Terrain and asset pipeline", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html", [
      [ "Overview", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md190", null ],
      [ "Source asset tree", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md191", null ],
      [ "World-map compilation", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md192", null ],
      [ "LRM1 map package", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md193", null ],
      [ "Geographic land index", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md194", null ],
      [ "DTED source selection", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md195", null ],
      [ "Supported DTED levels", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md196", null ],
      [ "DTED validation", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md197", null ],
      [ "Mosaic sampling and cache", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md198", null ],
      [ "Local land mask", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md199", null ],
      [ "Terrain patch construction", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md200", null ],
      [ "Asynchronous terrain lifecycle", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md201", null ],
      [ "Terrain rendering and overlays", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md202", null ],
      [ "glTF and GLB models", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md203", [
        [ "Model resource limits", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md204", null ]
      ] ],
      [ "Cubemap catalog", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md205", null ],
      [ "Build, stage, and install", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md206", null ],
      [ "Change checklist", "md_docs_2_t_e_r_r_a_i_n___a_n_d___a_s_s_e_t_s.html#autotoc_md207", null ]
    ] ],
    [ "Threat model", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html", [
      [ "Scope and security objectives", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html#autotoc_md209", null ],
      [ "Assets and adverse outcomes", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html#autotoc_md210", null ],
      [ "Actors and trust assumptions", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html#autotoc_md211", null ],
      [ "Trust boundaries and data flow", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html#autotoc_md212", null ],
      [ "Attack surfaces and controls", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html#autotoc_md213", [
        [ "UDP telemetry", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html#autotoc_md214", null ],
        [ "Mapping and LAR1 sessions", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html#autotoc_md215", null ],
        [ "glTF/GLB models and images", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html#autotoc_md216", null ],
        [ "Map, DTED, and terrain assets", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html#autotoc_md217", null ],
        [ "Rendering and extreme zoom", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html#autotoc_md218", null ],
        [ "Threads and shutdown", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html#autotoc_md219", null ],
        [ "Dependencies and plugins", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html#autotoc_md220", null ]
      ] ],
      [ "STRIDE summary", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html#autotoc_md221", null ],
      [ "Verification and review triggers", "md_docs_2_t_h_r_e_a_t___m_o_d_e_l.html#autotoc_md222", null ]
    ] ],
    [ "User guide", "md_docs_2_u_s_e_r___g_u_i_d_e.html", [
      [ "What the application does", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md224", null ],
      [ "Start the viewer", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md225", null ],
      [ "Window layout", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md226", null ],
      [ "Online capture", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md227", [
        [ "1. Load a packet mapping", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md228", null ],
        [ "2. Select the sender policy", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md229", null ],
        [ "3. Start the listener", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md230", null ],
        [ "4. Send test traffic", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md231", null ]
      ] ],
      [ "Recording", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md232", null ],
      [ "Offline replay", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md233", null ],
      [ "LAR views", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md234", [
        [ "Grid", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md235", null ],
        [ "Mercator", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md236", null ],
        [ "Sphere", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md237", null ],
        [ "Camera controls", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md238", null ]
      ] ],
      [ "Plane view", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md239", [
        [ "Load another model", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md240", null ],
        [ "Use terrain", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md241", null ]
      ] ],
      [ "DLZ view", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md242", null ],
      [ "Common problems", "md_docs_2_u_s_e_r___g_u_i_d_e.html#autotoc_md243", null ]
    ] ],
    [ "Visualization guide", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html", [
      [ "Scope", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md245", null ],
      [ "Coordinate and unit model", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md246", null ],
      [ "State-to-view pipeline", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md247", null ],
      [ "LAR input definitions", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md248", null ],
      [ "Grid view", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md249", null ],
      [ "Earth map pipeline", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md250", null ],
      [ "Mercator view", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md251", null ],
      [ "Sphere view", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md252", null ],
      [ "Geodesic zone accuracy and fallback", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md253", null ],
      [ "Camera behavior", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md254", null ],
      [ "Plane composition", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md255", [
        [ "Aircraft transform and scale", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md256", null ],
        [ "Tactical surface", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md257", null ],
        [ "Orbit camera", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md258", null ]
      ] ],
      [ "Terrain alignment", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md259", null ],
      [ "DLZ presentation", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md260", null ],
      [ "OpenGL lifecycle", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md261", null ],
      [ "Verification map", "md_docs_2_v_i_s_u_a_l_i_z_a_t_i_o_n.html#autotoc_md262", null ]
    ] ],
    [ "Topics", "topics.html", "topics" ],
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
"class_earth_lar_view.html#a4b140ea70c5516564e40b7f390d1599c",
"class_i_recording_file_dialog.html#ae91e2eab48474e6cd7f9fa166e1b175d",
"class_lar_zone_gpu_layer.html#a2c5749bc2206eff3d6b3a4b87c2bbdbb",
"class_persistence_runtime_worker.html#a3c423c02e3d0459af759b4e450f96971",
"class_plane_view_workspace.html#a2dd22be7496b2d94514d074a3869dbf1",
"class_recording_runtime_worker.html#aa95e5c13c8f695fd05b3228cd1027b4d",
"class_threaded_application_runtime.html#a202be05f809071b8c27a0ea8e23ebc02",
"classdlz_1_1presentation_1_1_control_panel.html#ada5ffd51470699b6d48882fbcb31f953",
"classlar_1_1map_1_1_earth_map_gpu_renderer.html#ac3a20ee0d6b9fc799a46e389f4b563f9",
"classlar_1_1map_1_1_packaged_map_asset_source.html#a490c4365096d0979f82c293bfab2131d",
"functions_vars_s.html",
"md_docs_2_c_o_m_p_o_n_e_n_t___r_e_f_e_r_e_n_c_e.html#autotoc_md53",
"namespace_test_sender_scenarios_1_1anonymous__namespace_02scenarios_8cpp_03.html#a1e2e37c81b9bfe26c18c337258ce5cd1",
"namespaceanonymous__namespace_02plane__surface__gpu__layer_8cpp_03.html#a9c2b151cabf6f5a80e36174a748cbab4",
"namespacelar_1_1map_1_1limits.html#aa6614c1376644c38938b5340cf107c0d",
"plane__model__mesh_8h.html#abdcd07f26c5b56302dcb9d16ce9b18aa",
"struct_captured_packet.html#afd8b57084275b8c2001d6dff14b10700",
"struct_plane_terrain_patch.html#a3890c114204c63d9304c7f3e0ea9afee",
"structdlz_1_1_scenario_frame.html#ac90e52e7e6a7c2b59cc1b65feba32135",
"threaded__application__runtime_8cpp.html"
];

const SYNCONMSG = 'click to disable panel synchronization';
const SYNCOFFMSG = 'click to enable panel synchronization';
const LISTOFALLMEMBERS = 'List of all members';