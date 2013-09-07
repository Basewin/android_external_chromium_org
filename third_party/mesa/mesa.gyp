# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'generated_src_dir': 'src/chromium_gensrc',
  },
  'target_defaults': {
    'defines': [
      'MAPI_ABI_HEADER="glapi_mapi_tmp_shared.h"',
      "PACKAGE_NAME=\"Mesa\"",
      "PACKAGE_TARNAME=\"mesa\"",
      "PACKAGE_VERSION=\"9.0.3\"",
      "PACKAGE_STRING=\"Mesa\ 9.0.3\"",
      "PACKAGE_BUGREPORT=\"https://bugs.freedesktop.org/enter_bug.cgi\?product=Mesa\"",
      "PACKAGE_URL=\"\"",
      "PACKAGE=\"mesa\"",
      "VERSION=\"9.0.3\"",
      "STDC_HEADERS=1",
      "HAVE_SYS_TYPES_H=1",
      "HAVE_SYS_STAT_H=1",
      "HAVE_STDLIB_H=1",
      "HAVE_STRING_H=1",
      "HAVE_MEMORY_H=1",
      "HAVE_STRINGS_H=1",
      "HAVE_INTTYPES_H=1",
      "HAVE_STDINT_H=1",
      "HAVE_DLFCN_H=1",
      "LT_OBJDIR=\".libs/\"",
      "YYTEXT_POINTER=1",
      "HAVE_LIBEXPAT=1",
      "HAVE_LIBXCB_DRI2=1",
      "FEATURE_GL=1",
      'MAPI_MODE_GLAPI',
      #"USE_X86_64_ASM",
      "IN_DRI_DRIVER",
      "USE_XCB",
      "GLX_INDIRECT_RENDERING",
      "GLX_DIRECT_RENDERING",
      "USE_EXTERNAL_DXTN_LIB=1",
      "IN_DRI_DRIVER",
      "HAVE_ALIAS",
      "HAVE_MINCORE",
      "HAVE_LIBUDEV",
      "_GLAPI_NO_EXPORTS",
    ],
    'conditions': [
      ['OS=="android"', {
        'defines': [
          '__GLIBC__',
          '_GNU_SOURCE',
        ],
      }],
      ['OS=="linux"', {
        'defines': [
          '_GNU_SOURCE',
        ],
      }],
      ['os_posix == 1', {
        'defines': [
          'HAVE_DLOPEN',
          'HAVE_PTHREAD=1',
          'HAVE_UNISTD_H=1',
        ],
      }],
      ['os_posix == 1 and OS != "android"', {
        'defines': [
          'HAVE_POSIX_MEMALIGN',
        ],
      }],
      ['os_posix == 1 and OS != "mac" and OS != "android"', {
        'cflags': [
          '-fPIC',
        ],
      }],
      ['OS=="win"', {
        # Pick up emulation headers not supported by Visual Studio.
        'include_dirs': [
          'src/include/c99',
        ],
      }],
    ],
  },
  'targets': [
    {
      'target_name': 'mesa_headers',
      'type': 'none',
      'direct_dependent_settings': {
        'include_dirs': [
          'src/include',
        ],
        'xcode_settings': {
          'WARNING_CFLAGS': [
            '-Wno-unknown-pragmas',
          ],
        },
        'cflags': [
          '-Wno-unknown-pragmas',
        ],
      },
      'conditions': [
        ['use_x11==0', {
          'direct_dependent_settings': {
            'defines': [
              'MESA_EGL_NO_X11_HEADERS',
            ],
          },
        }],
      ],
    },
    {
      'target_name': 'mesa_libglslcommon',
      'type': 'static_library',
      'include_dirs': [
        'src/src/gallium/auxiliary',
        'src/src/gallium/include',
        'src/src/glsl',
        'src/src/glsl/glcpp',
        'src/src/mapi',
        'src/src/mapi/glapi',
        'src/src/mesa',
        'src/src/mesa/main',
        'src/include',
        '<(generated_src_dir)/mesa/',
        '<(generated_src_dir)/mesa/main',
        '<(generated_src_dir)/mesa/program',
        '<(generated_src_dir)/mesa/glapi',
      ],
      'dependencies': [
        'mesa_headers',
      ],
      # TODO(scottmg): http://crbug.com/143877 These should be removed if
      # Mesa is ever rolled and the warnings are fixed.
      'msvs_disabled_warnings': [
          4005, 4018, 4065, 4090, 4099, 4273, 4291, 4345, 4267,
      ],
      'sources': [
        '<(generated_src_dir)/mesa/main/dispatch.h',
        'src/src/glsl/ast_expr.cpp',
        'src/src/glsl/ast_function.cpp',
        'src/src/glsl/ast_to_hir.cpp',
        'src/src/glsl/ast_type.cpp',
        'src/src/glsl/builtin_variables.cpp',
        '<(generated_src_dir)/mesa/glcpp-lex.c',
        '<(generated_src_dir)/mesa/glcpp-parse.c',
        '<(generated_src_dir)/mesa/glcpp-parse.h',
        'src/src/glsl/glcpp/glcpp.h',
        'src/src/glsl/glcpp/pp.c',
        '<(generated_src_dir)/mesa/glsl_lexer.cc',
        '<(generated_src_dir)/mesa/glsl_parser.cc',
        'src/src/glsl/glsl_parser_extras.cpp',
        'src/src/glsl/glsl_parser_extras.h',
        'src/src/glsl/glsl_symbol_table.cpp',
        'src/src/glsl/glsl_symbol_table.h',
        'src/src/glsl/glsl_types.cpp',
        'src/src/glsl/glsl_types.h',
        'src/src/glsl/hir_field_selection.cpp',
        'src/src/glsl/ir.cpp',
        'src/src/glsl/ir.h',
        'src/src/glsl/ir_basic_block.cpp',
        'src/src/glsl/ir_basic_block.h',
        'src/src/glsl/ir_builder.cpp',
        'src/src/glsl/ir_builder.h',
        'src/src/glsl/ir_clone.cpp',
        'src/src/glsl/ir_constant_expression.cpp',
        'src/src/glsl/ir_expression_flattening.cpp',
        'src/src/glsl/ir_expression_flattening.h',
        'src/src/glsl/ir_function.cpp',
        'src/src/glsl/ir_function_can_inline.cpp',
        'src/src/glsl/ir_function_detect_recursion.cpp',
        'src/src/glsl/ir_hierarchical_visitor.cpp',
        'src/src/glsl/ir_hierarchical_visitor.h',
        'src/src/glsl/ir_hv_accept.cpp',
        'src/src/glsl/ir_import_prototypes.cpp',
        'src/src/glsl/ir_print_visitor.cpp',
        'src/src/glsl/ir_print_visitor.h',
        'src/src/glsl/ir_reader.cpp',
        'src/src/glsl/ir_reader.h',
        'src/src/glsl/ir_rvalue_visitor.cpp',
        'src/src/glsl/ir_rvalue_visitor.h',
        'src/src/glsl/ir_set_program_inouts.cpp',
        'src/src/glsl/ir_validate.cpp',
        'src/src/glsl/ir_variable_refcount.cpp',
        'src/src/glsl/ir_variable_refcount.h',
        'src/src/glsl/link_functions.cpp',
        'src/src/glsl/link_uniform_initializers.cpp',
        'src/src/glsl/link_uniforms.cpp',
        'src/src/glsl/linker.cpp',
        'src/src/glsl/linker.h',
        'src/src/glsl/loop_analysis.cpp',
        'src/src/glsl/loop_analysis.h',
        'src/src/glsl/loop_controls.cpp',
        'src/src/glsl/loop_unroll.cpp',
        'src/src/glsl/lower_clip_distance.cpp',
        'src/src/glsl/lower_discard.cpp',
        'src/src/glsl/lower_discard_flow.cpp',
        'src/src/glsl/lower_if_to_cond_assign.cpp',
        'src/src/glsl/lower_instructions.cpp',
        'src/src/glsl/lower_jumps.cpp',
        'src/src/glsl/lower_mat_op_to_vec.cpp',
        'src/src/glsl/lower_noise.cpp',
        'src/src/glsl/lower_output_reads.cpp',
        'src/src/glsl/lower_texture_projection.cpp',
        'src/src/glsl/lower_ubo_reference.cpp',
        'src/src/glsl/lower_variable_index_to_cond_assign.cpp',
        'src/src/glsl/lower_vec_index_to_cond_assign.cpp',
        'src/src/glsl/lower_vec_index_to_swizzle.cpp',
        'src/src/glsl/lower_vector.cpp',
        'src/src/glsl/opt_algebraic.cpp',
        'src/src/glsl/opt_array_splitting.cpp',
        'src/src/glsl/opt_constant_folding.cpp',
        'src/src/glsl/opt_constant_propagation.cpp',
        'src/src/glsl/opt_constant_variable.cpp',
        'src/src/glsl/opt_copy_propagation.cpp',
        'src/src/glsl/opt_copy_propagation_elements.cpp',
        'src/src/glsl/opt_dead_code.cpp',
        'src/src/glsl/opt_dead_code_local.cpp',
        'src/src/glsl/opt_dead_functions.cpp',
        'src/src/glsl/opt_function_inlining.cpp',
        'src/src/glsl/opt_if_simplification.cpp',
        'src/src/glsl/opt_noop_swizzle.cpp',
        'src/src/glsl/opt_redundant_jumps.cpp',
        'src/src/glsl/opt_structure_splitting.cpp',
        'src/src/glsl/opt_swizzle_swizzle.cpp',
        'src/src/glsl/opt_tree_grafting.cpp',
        'src/src/glsl/program.h',
        'src/src/glsl/ralloc.c',
        'src/src/glsl/ralloc.h',
        'src/src/glsl/s_expression.cpp',
        'src/src/glsl/s_expression.h',
        'src/src/glsl/standalone_scaffolding.cpp',
        'src/src/glsl/standalone_scaffolding.h',
        'src/src/glsl/strtod.c',
        'src/src/glsl/strtod.h',
      ],
      'conditions': [
        ['clang == 1', {
          'xcode_settings': {
            'WARNING_CFLAGS': [
              '-Wno-tautological-constant-out-of-range-compare',
            ],
            'WARNING_CFLAGS!': [
              # Don't warn about string->bool used in asserts.
              '-Wstring-conversion',
            ],
          },
          'cflags': [
            '-Wno-tautological-constant-out-of-range-compare',
          ],
          'cflags!': [
            '-Wstring-conversion',
          ],
        }],
      ],
    },
    {
      'target_name': 'mesa',
      'type': 'static_library',
      'include_dirs': [
        'src/src/gallium/auxiliary',
        'src/src/gallium/include',
        'src/src/glsl',
        'src/src/glsl/glcpp',
        'src/src/mapi',
        'src/src/mapi/glapi',
        'src/src/mesa',
        'src/src/mesa/main',
        '<(generated_src_dir)/mesa/',
        '<(generated_src_dir)/mesa/main',
        '<(generated_src_dir)/mesa/program',
        '<(generated_src_dir)/mesa/glapi',
      ],
      'dependencies': [
        'mesa_headers',
        'mesa_libglslcommon',
      ],
      # TODO(scottmg): http://crbug.com/143877 These should be removed if
      # Mesa is ever rolled and the warnings are fixed.
      'msvs_disabled_warnings': [
          4005, 4018, 4090, 4099, 4146, 4273, 4291, 4305, 4334, 4748, 4267,
      ],
      'sources': [
        '<(generated_src_dir)/mesa/builtin_function.cpp',
        '<(generated_src_dir)/mesa/glapi_mapi_tmp_shared.h',
        'src/src/mapi/mapi/entry.c',
        'src/src/mapi/mapi/entry.h',
        'src/src/mapi/mapi/mapi.c',
        'src/src/mapi/mapi/mapi.h',
        'src/src/mapi/mapi/mapi_glapi.c',
        'src/src/mapi/mapi/stub.c',
        'src/src/mapi/mapi/stub.h',
        'src/src/mapi/mapi/table.c',
        'src/src/mapi/mapi/table.h',
        'src/src/mapi/mapi/u_current.c',
        'src/src/mapi/mapi/u_current.h',
        'src/src/mapi/mapi/u_execmem.c',
        'src/src/mapi/mapi/u_execmem.h',
        'src/src/mesa/main/accum.c',
        'src/src/mesa/main/accum.h',
        'src/src/mesa/main/api_arrayelt.c',
        'src/src/mesa/main/api_arrayelt.h',
        'src/src/mesa/main/api_exec.c',
        'src/src/mesa/main/api_exec.h',
        '<(generated_src_dir)/mesa/api_exec_es1.c',
        'src/src/mesa/main/api_loopback.c',
        'src/src/mesa/main/api_loopback.h',
        'src/src/mesa/main/api_validate.c',
        'src/src/mesa/main/api_validate.h',
        'src/src/mesa/main/arbprogram.c',
        'src/src/mesa/main/arbprogram.h',
        'src/src/mesa/main/arrayobj.c',
        'src/src/mesa/main/arrayobj.h',
        'src/src/mesa/main/atifragshader.c',
        'src/src/mesa/main/atifragshader.h',
        'src/src/mesa/main/attrib.c',
        'src/src/mesa/main/attrib.h',
        'src/src/mesa/main/blend.c',
        'src/src/mesa/main/blend.h',
        'src/src/mesa/main/bufferobj.c',
        'src/src/mesa/main/bufferobj.h',
        'src/src/mesa/main/buffers.c',
        'src/src/mesa/main/buffers.h',
        'src/src/mesa/main/clear.c',
        'src/src/mesa/main/clear.h',
        'src/src/mesa/main/clip.c',
        'src/src/mesa/main/clip.h',
        'src/src/mesa/main/colortab.c',
        'src/src/mesa/main/colortab.h',
        'src/src/mesa/main/condrender.c',
        'src/src/mesa/main/condrender.h',
        'src/src/mesa/main/context.c',
        'src/src/mesa/main/context.h',
        'src/src/mesa/main/convolve.c',
        'src/src/mesa/main/convolve.h',
        'src/src/mesa/main/cpuinfo.c',
        'src/src/mesa/main/cpuinfo.h',
        'src/src/mesa/main/debug.c',
        'src/src/mesa/main/debug.h',
        'src/src/mesa/main/depth.c',
        'src/src/mesa/main/depth.h',
        'src/src/mesa/main/dlist.c',
        'src/src/mesa/main/dlist.h',
        'src/src/mesa/main/drawpix.c',
        'src/src/mesa/main/drawpix.h',
        'src/src/mesa/main/drawtex.c',
        'src/src/mesa/main/drawtex.h',
        'src/src/mesa/main/enable.c',
        'src/src/mesa/main/enable.h',
        '<(generated_src_dir)/mesa/enums.c',
        'src/src/mesa/main/enums.h',
        'src/src/mesa/main/errors.c',
        'src/src/mesa/main/errors.h',
        'src/src/mesa/main/es1_conversion.c',
        'src/src/mesa/main/es1_conversion.h',
        'src/src/mesa/main/eval.c',
        'src/src/mesa/main/eval.h',
        'src/src/mesa/main/execmem.c',
        'src/src/mesa/main/extensions.c',
        'src/src/mesa/main/extensions.h',
        'src/src/mesa/main/fbobject.c',
        'src/src/mesa/main/fbobject.h',
        'src/src/mesa/main/feedback.c',
        'src/src/mesa/main/feedback.h',
        'src/src/mesa/main/ff_fragment_shader.cpp',
        'src/src/mesa/main/ffvertex_prog.c',
        'src/src/mesa/main/ffvertex_prog.h',
        'src/src/mesa/main/fog.c',
        'src/src/mesa/main/fog.h',
        'src/src/mesa/main/format_pack.c',
        'src/src/mesa/main/format_pack.h',
        'src/src/mesa/main/format_unpack.c',
        'src/src/mesa/main/format_unpack.h',
        'src/src/mesa/main/formats.c',
        'src/src/mesa/main/formats.h',
        'src/src/mesa/main/framebuffer.c',
        'src/src/mesa/main/framebuffer.h',
        'src/src/mesa/main/get.c',
        'src/src/mesa/main/get.h',
        'src/src/mesa/main/getstring.c',
        'src/src/mesa/main/glformats.c',
        'src/src/mesa/main/glformats.h',
        'src/src/mesa/main/hash.c',
        'src/src/mesa/main/hash.h',
        'src/src/mesa/main/hint.c',
        'src/src/mesa/main/hint.h',
        'src/src/mesa/main/histogram.c',
        'src/src/mesa/main/histogram.h',
        'src/src/mesa/main/image.c',
        'src/src/mesa/main/image.h',
        'src/src/mesa/main/imports.c',
        'src/src/mesa/main/imports.h',
        'src/src/mesa/main/light.c',
        'src/src/mesa/main/light.h',
        'src/src/mesa/main/lines.c',
        'src/src/mesa/main/lines.h',
        'src/src/mesa/main/matrix.c',
        'src/src/mesa/main/matrix.h',
        'src/src/mesa/main/mipmap.c',
        'src/src/mesa/main/mipmap.h',
        'src/src/mesa/main/mm.c',
        'src/src/mesa/main/mm.h',
        'src/src/mesa/main/multisample.c',
        'src/src/mesa/main/multisample.h',
        'src/src/mesa/main/nvprogram.c',
        'src/src/mesa/main/nvprogram.h',
        'src/src/mesa/main/pack.c',
        'src/src/mesa/main/pack.h',
        'src/src/mesa/main/pbo.c',
        'src/src/mesa/main/pbo.h',
        'src/src/mesa/main/pixel.c',
        'src/src/mesa/main/pixel.h',
        'src/src/mesa/main/pixelstore.c',
        'src/src/mesa/main/pixelstore.h',
        'src/src/mesa/main/pixeltransfer.c',
        'src/src/mesa/main/pixeltransfer.h',
        'src/src/mesa/main/points.c',
        'src/src/mesa/main/points.h',
        'src/src/mesa/main/polygon.c',
        'src/src/mesa/main/polygon.h',
        'src/src/mesa/main/querymatrix.c',
        'src/src/mesa/main/queryobj.c',
        'src/src/mesa/main/queryobj.h',
        'src/src/mesa/main/rastpos.c',
        'src/src/mesa/main/rastpos.h',
        'src/src/mesa/main/readpix.c',
        'src/src/mesa/main/readpix.h',
        'src/src/mesa/main/remap.c',
        'src/src/mesa/main/remap.h',
        'src/src/mesa/main/renderbuffer.c',
        'src/src/mesa/main/renderbuffer.h',
        'src/src/mesa/main/samplerobj.c',
        'src/src/mesa/main/samplerobj.h',
        'src/src/mesa/main/scissor.c',
        'src/src/mesa/main/scissor.h',
        'src/src/mesa/main/shader_query.cpp',
        'src/src/mesa/main/shaderapi.c',
        'src/src/mesa/main/shaderapi.h',
        'src/src/mesa/main/shaderobj.c',
        'src/src/mesa/main/shaderobj.h',
        'src/src/mesa/main/shared.c',
        'src/src/mesa/main/shared.h',
        'src/src/mesa/main/state.c',
        'src/src/mesa/main/state.h',
        'src/src/mesa/main/stencil.c',
        'src/src/mesa/main/stencil.h',
        'src/src/mesa/main/syncobj.c',
        'src/src/mesa/main/syncobj.h',
        'src/src/mesa/main/texcompress.c',
        'src/src/mesa/main/texcompress.h',
        'src/src/mesa/main/texcompress_cpal.c',
        'src/src/mesa/main/texcompress_cpal.h',
        'src/src/mesa/main/texcompress_etc.c',
        'src/src/mesa/main/texcompress_etc.h',
        'src/src/mesa/main/texcompress_fxt1.c',
        'src/src/mesa/main/texcompress_fxt1.h',
        'src/src/mesa/main/texcompress_rgtc.c',
        'src/src/mesa/main/texcompress_rgtc.h',
        'src/src/mesa/main/texcompress_s3tc.c',
        'src/src/mesa/main/texcompress_s3tc.h',
        'src/src/mesa/main/texenv.c',
        'src/src/mesa/main/texenv.h',
        'src/src/mesa/main/texformat.c',
        'src/src/mesa/main/texformat.h',
        'src/src/mesa/main/texgen.c',
        'src/src/mesa/main/texgen.h',
        'src/src/mesa/main/texgetimage.c',
        'src/src/mesa/main/texgetimage.h',
        'src/src/mesa/main/teximage.c',
        'src/src/mesa/main/teximage.h',
        'src/src/mesa/main/texobj.c',
        'src/src/mesa/main/texobj.h',
        'src/src/mesa/main/texparam.c',
        'src/src/mesa/main/texparam.h',
        'src/src/mesa/main/texstate.c',
        'src/src/mesa/main/texstate.h',
        'src/src/mesa/main/texstorage.c',
        'src/src/mesa/main/texstorage.h',
        'src/src/mesa/main/texstore.c',
        'src/src/mesa/main/texstore.h',
        'src/src/mesa/main/texturebarrier.c',
        'src/src/mesa/main/texturebarrier.h',
        'src/src/mesa/main/transformfeedback.c',
        'src/src/mesa/main/transformfeedback.h',
        'src/src/mesa/main/uniform_query.cpp',
        'src/src/mesa/main/uniforms.c',
        'src/src/mesa/main/uniforms.h',
        'src/src/mesa/main/varray.c',
        'src/src/mesa/main/varray.h',
        'src/src/mesa/main/version.c',
        'src/src/mesa/main/version.h',
        'src/src/mesa/main/viewport.c',
        'src/src/mesa/main/viewport.h',
        'src/src/mesa/main/vtxfmt.c',
        'src/src/mesa/main/vtxfmt.h',
        'src/src/mesa/math/m_debug_clip.c',
        'src/src/mesa/math/m_debug_norm.c',
        'src/src/mesa/math/m_debug_xform.c',
        'src/src/mesa/math/m_eval.c',
        'src/src/mesa/math/m_eval.h',
        'src/src/mesa/math/m_matrix.c',
        'src/src/mesa/math/m_matrix.h',
        'src/src/mesa/math/m_translate.c',
        'src/src/mesa/math/m_translate.h',
        'src/src/mesa/math/m_vector.c',
        'src/src/mesa/math/m_vector.h',
        'src/src/mesa/math/m_xform.c',
        'src/src/mesa/math/m_xform.h',
        'src/src/mesa/program/arbprogparse.c',
        'src/src/mesa/program/arbprogparse.h',
        'src/src/mesa/program/hash_table.c',
        'src/src/mesa/program/hash_table.h',
        'src/src/mesa/program/ir_to_mesa.cpp',
        'src/src/mesa/program/ir_to_mesa.h',
        '<(generated_src_dir)/mesa/lex.yy.c',
        'src/src/mesa/program/nvfragparse.c',
        'src/src/mesa/program/nvfragparse.h',
        'src/src/mesa/program/nvvertparse.c',
        'src/src/mesa/program/nvvertparse.h',
        'src/src/mesa/program/prog_cache.c',
        'src/src/mesa/program/prog_cache.h',
        'src/src/mesa/program/prog_execute.c',
        'src/src/mesa/program/prog_execute.h',
        'src/src/mesa/program/prog_instruction.c',
        'src/src/mesa/program/prog_instruction.h',
        'src/src/mesa/program/prog_noise.c',
        'src/src/mesa/program/prog_noise.h',
        'src/src/mesa/program/prog_opt_constant_fold.c',
        'src/src/mesa/program/prog_optimize.c',
        'src/src/mesa/program/prog_optimize.h',
        'src/src/mesa/program/prog_parameter.c',
        'src/src/mesa/program/prog_parameter.h',
        'src/src/mesa/program/prog_parameter_layout.c',
        'src/src/mesa/program/prog_parameter_layout.h',
        'src/src/mesa/program/prog_print.c',
        'src/src/mesa/program/prog_print.h',
        'src/src/mesa/program/prog_statevars.c',
        'src/src/mesa/program/prog_statevars.h',
        'src/src/mesa/program/program.c',
        'src/src/mesa/program/program.h',
        '<(generated_src_dir)/mesa/program/program_parse.tab.c',
        '<(generated_src_dir)/mesa/program/program_parse.tab.h',
        'src/src/mesa/program/program_parse_extra.c',
        'src/src/mesa/program/programopt.c',
        'src/src/mesa/program/programopt.h',
        'src/src/mesa/program/register_allocate.c',
        'src/src/mesa/program/register_allocate.h',
        'src/src/mesa/program/sampler.cpp',
        'src/src/mesa/program/sampler.h',
        'src/src/mesa/program/string_to_uint_map.cpp',
        'src/src/mesa/program/symbol_table.c',
        'src/src/mesa/program/symbol_table.h',
        'src/src/mesa/state_tracker/st_atom.c',
        'src/src/mesa/state_tracker/st_atom.h',
        'src/src/mesa/state_tracker/st_atom_array.c',
        'src/src/mesa/state_tracker/st_atom_blend.c',
        'src/src/mesa/state_tracker/st_atom_clip.c',
        'src/src/mesa/state_tracker/st_atom_constbuf.c',
        'src/src/mesa/state_tracker/st_atom_constbuf.h',
        'src/src/mesa/state_tracker/st_atom_depth.c',
        'src/src/mesa/state_tracker/st_atom_framebuffer.c',
        'src/src/mesa/state_tracker/st_atom_msaa.c',
        'src/src/mesa/state_tracker/st_atom_pixeltransfer.c',
        'src/src/mesa/state_tracker/st_atom_rasterizer.c',
        'src/src/mesa/state_tracker/st_atom_sampler.c',
        'src/src/mesa/state_tracker/st_atom_scissor.c',
        'src/src/mesa/state_tracker/st_atom_shader.c',
        'src/src/mesa/state_tracker/st_atom_shader.h',
        'src/src/mesa/state_tracker/st_atom_stipple.c',
        'src/src/mesa/state_tracker/st_atom_texture.c',
        'src/src/mesa/state_tracker/st_atom_viewport.c',
        'src/src/mesa/state_tracker/st_cb_bitmap.c',
        'src/src/mesa/state_tracker/st_cb_bitmap.h',
        'src/src/mesa/state_tracker/st_cb_blit.c',
        'src/src/mesa/state_tracker/st_cb_blit.h',
        'src/src/mesa/state_tracker/st_cb_bufferobjects.c',
        'src/src/mesa/state_tracker/st_cb_bufferobjects.h',
        'src/src/mesa/state_tracker/st_cb_clear.c',
        'src/src/mesa/state_tracker/st_cb_clear.h',
        'src/src/mesa/state_tracker/st_cb_condrender.c',
        'src/src/mesa/state_tracker/st_cb_condrender.h',
        'src/src/mesa/state_tracker/st_cb_drawpixels.c',
        'src/src/mesa/state_tracker/st_cb_drawpixels.h',
        'src/src/mesa/state_tracker/st_cb_drawtex.c',
        'src/src/mesa/state_tracker/st_cb_drawtex.h',
        'src/src/mesa/state_tracker/st_cb_eglimage.c',
        'src/src/mesa/state_tracker/st_cb_eglimage.h',
        'src/src/mesa/state_tracker/st_cb_fbo.c',
        'src/src/mesa/state_tracker/st_cb_fbo.h',
        'src/src/mesa/state_tracker/st_cb_feedback.c',
        'src/src/mesa/state_tracker/st_cb_feedback.h',
        'src/src/mesa/state_tracker/st_cb_flush.c',
        'src/src/mesa/state_tracker/st_cb_flush.h',
        'src/src/mesa/state_tracker/st_cb_program.c',
        'src/src/mesa/state_tracker/st_cb_program.h',
        'src/src/mesa/state_tracker/st_cb_queryobj.c',
        'src/src/mesa/state_tracker/st_cb_queryobj.h',
        'src/src/mesa/state_tracker/st_cb_rasterpos.c',
        'src/src/mesa/state_tracker/st_cb_rasterpos.h',
        'src/src/mesa/state_tracker/st_cb_readpixels.c',
        'src/src/mesa/state_tracker/st_cb_readpixels.h',
        'src/src/mesa/state_tracker/st_cb_strings.c',
        'src/src/mesa/state_tracker/st_cb_strings.h',
        'src/src/mesa/state_tracker/st_cb_syncobj.c',
        'src/src/mesa/state_tracker/st_cb_syncobj.h',
        'src/src/mesa/state_tracker/st_cb_texture.c',
        'src/src/mesa/state_tracker/st_cb_texture.h',
        'src/src/mesa/state_tracker/st_cb_texturebarrier.c',
        'src/src/mesa/state_tracker/st_cb_texturebarrier.h',
        'src/src/mesa/state_tracker/st_cb_viewport.c',
        'src/src/mesa/state_tracker/st_cb_viewport.h',
        'src/src/mesa/state_tracker/st_cb_xformfb.c',
        'src/src/mesa/state_tracker/st_cb_xformfb.h',
        'src/src/mesa/state_tracker/st_context.c',
        'src/src/mesa/state_tracker/st_context.h',
        'src/src/mesa/state_tracker/st_debug.c',
        'src/src/mesa/state_tracker/st_debug.h',
        'src/src/mesa/state_tracker/st_draw.c',
        'src/src/mesa/state_tracker/st_draw.h',
        'src/src/mesa/state_tracker/st_draw_feedback.c',
        'src/src/mesa/state_tracker/st_extensions.c',
        'src/src/mesa/state_tracker/st_extensions.h',
        'src/src/mesa/state_tracker/st_format.c',
        'src/src/mesa/state_tracker/st_format.h',
        'src/src/mesa/state_tracker/st_gen_mipmap.c',
        'src/src/mesa/state_tracker/st_gen_mipmap.h',
        'src/src/mesa/state_tracker/st_glsl_to_tgsi.cpp',
        'src/src/mesa/state_tracker/st_glsl_to_tgsi.h',
        'src/src/mesa/state_tracker/st_manager.c',
        'src/src/mesa/state_tracker/st_manager.h',
        'src/src/mesa/state_tracker/st_mesa_to_tgsi.c',
        'src/src/mesa/state_tracker/st_mesa_to_tgsi.h',
        'src/src/mesa/state_tracker/st_program.c',
        'src/src/mesa/state_tracker/st_program.h',
        'src/src/mesa/state_tracker/st_texture.c',
        'src/src/mesa/state_tracker/st_texture.h',
        'src/src/mesa/swrast/s_aaline.c',
        'src/src/mesa/swrast/s_aaline.h',
        'src/src/mesa/swrast/s_aatriangle.c',
        'src/src/mesa/swrast/s_aatriangle.h',
        'src/src/mesa/swrast/s_alpha.c',
        'src/src/mesa/swrast/s_alpha.h',
        'src/src/mesa/swrast/s_atifragshader.c',
        'src/src/mesa/swrast/s_atifragshader.h',
        'src/src/mesa/swrast/s_bitmap.c',
        'src/src/mesa/swrast/s_blend.c',
        'src/src/mesa/swrast/s_blend.h',
        'src/src/mesa/swrast/s_blit.c',
        'src/src/mesa/swrast/s_clear.c',
        'src/src/mesa/swrast/s_context.c',
        'src/src/mesa/swrast/s_context.h',
        'src/src/mesa/swrast/s_copypix.c',
        'src/src/mesa/swrast/s_depth.c',
        'src/src/mesa/swrast/s_depth.h',
        'src/src/mesa/swrast/s_drawpix.c',
        'src/src/mesa/swrast/s_feedback.c',
        'src/src/mesa/swrast/s_feedback.h',
        'src/src/mesa/swrast/s_fog.c',
        'src/src/mesa/swrast/s_fog.h',
        'src/src/mesa/swrast/s_fragprog.c',
        'src/src/mesa/swrast/s_fragprog.h',
        'src/src/mesa/swrast/s_lines.c',
        'src/src/mesa/swrast/s_lines.h',
        'src/src/mesa/swrast/s_logic.c',
        'src/src/mesa/swrast/s_logic.h',
        'src/src/mesa/swrast/s_masking.c',
        'src/src/mesa/swrast/s_masking.h',
        'src/src/mesa/swrast/s_points.c',
        'src/src/mesa/swrast/s_points.h',
        'src/src/mesa/swrast/s_renderbuffer.c',
        'src/src/mesa/swrast/s_renderbuffer.h',
        'src/src/mesa/swrast/s_span.c',
        'src/src/mesa/swrast/s_span.h',
        'src/src/mesa/swrast/s_stencil.c',
        'src/src/mesa/swrast/s_stencil.h',
        'src/src/mesa/swrast/s_texcombine.c',
        'src/src/mesa/swrast/s_texcombine.h',
        'src/src/mesa/swrast/s_texfetch.c',
        'src/src/mesa/swrast/s_texfetch.h',
        'src/src/mesa/swrast/s_texfilter.c',
        'src/src/mesa/swrast/s_texfilter.h',
        'src/src/mesa/swrast/s_texrender.c',
        'src/src/mesa/swrast/s_texture.c',
        'src/src/mesa/swrast/s_triangle.c',
        'src/src/mesa/swrast/s_triangle.h',
        'src/src/mesa/swrast/s_zoom.c',
        'src/src/mesa/swrast/s_zoom.h',
        'src/src/mesa/swrast_setup/ss_context.c',
        'src/src/mesa/swrast_setup/ss_context.h',
        'src/src/mesa/swrast_setup/ss_triangle.c',
        'src/src/mesa/swrast_setup/ss_triangle.h',
        'src/src/mesa/tnl/t_context.c',
        'src/src/mesa/tnl/t_context.h',
        'src/src/mesa/tnl/t_draw.c',
        'src/src/mesa/tnl/t_pipeline.c',
        'src/src/mesa/tnl/t_pipeline.h',
        'src/src/mesa/tnl/t_rasterpos.c',
        'src/src/mesa/tnl/t_vb_fog.c',
        'src/src/mesa/tnl/t_vb_light.c',
        'src/src/mesa/tnl/t_vb_normals.c',
        'src/src/mesa/tnl/t_vb_points.c',
        'src/src/mesa/tnl/t_vb_program.c',
        'src/src/mesa/tnl/t_vb_render.c',
        'src/src/mesa/tnl/t_vb_texgen.c',
        'src/src/mesa/tnl/t_vb_texmat.c',
        'src/src/mesa/tnl/t_vb_vertex.c',
        'src/src/mesa/tnl/t_vertex.c',
        'src/src/mesa/tnl/t_vertex.h',
        'src/src/mesa/tnl/t_vertex_generic.c',
        'src/src/mesa/tnl/t_vertex_sse.c',
        'src/src/mesa/tnl/t_vp_build.c',
        'src/src/mesa/tnl/t_vp_build.h',
        'src/src/mesa/vbo/vbo_context.c',
        'src/src/mesa/vbo/vbo_context.h',
        'src/src/mesa/vbo/vbo_exec.c',
        'src/src/mesa/vbo/vbo_exec.h',
        'src/src/mesa/vbo/vbo_exec_api.c',
        'src/src/mesa/vbo/vbo_exec_array.c',
        'src/src/mesa/vbo/vbo_exec_draw.c',
        'src/src/mesa/vbo/vbo_exec_eval.c',
        'src/src/mesa/vbo/vbo_noop.c',
        'src/src/mesa/vbo/vbo_noop.h',
        'src/src/mesa/vbo/vbo_primitive_restart.c',
        'src/src/mesa/vbo/vbo_rebase.c',
        'src/src/mesa/vbo/vbo_save.c',
        'src/src/mesa/vbo/vbo_save.h',
        'src/src/mesa/vbo/vbo_save_api.c',
        'src/src/mesa/vbo/vbo_save_draw.c',
        'src/src/mesa/vbo/vbo_save_loopback.c',
        'src/src/mesa/vbo/vbo_split.c',
        'src/src/mesa/vbo/vbo_split.h',
        'src/src/mesa/vbo/vbo_split_copy.c',
        'src/src/mesa/vbo/vbo_split_inplace.c',
        'src/src/mesa/x86-64/x86-64.c',
        'src/src/mesa/x86-64/x86-64.h',
      ],
      'conditions': [
        ['clang == 1', {
          'xcode_settings': {
            'WARNING_CFLAGS': [
              '-Wno-tautological-constant-out-of-range-compare',
            ],
            'WARNING_CFLAGS!': [
              # Don't warn about string->bool used in asserts.
              '-Wstring-conversion',
            ],
          },
          'cflags': [
            '-Wno-tautological-constant-out-of-range-compare',
          ],
          'cflags!': [
            '-Wstring-conversion',
          ],
        }],
        ['OS=="android" and clang==0', {
          # Disable sincos() optimization to avoid a linker error
          # since Android's math library doesn't have sincos().
          # Either -fno-builtin-sin or -fno-builtin-cos works.
          'cflags': [
            '-fno-builtin-sin',
          ],
        }],
        ['OS=="win"', {
          'defines': [
            # Because we're building as a static library
            '_GLAPI_NO_EXPORTS',
          ],
        }],
      ],
    },
    # Building this target will hide the native OpenGL shared library and
    # replace it with a slow software renderer.
    {
      'target_name': 'osmesa',
      'type': 'loadable_module',
      'mac_bundle': 0,
      'dependencies': [
        'mesa_headers',
        'mesa',
      ],
      'xcode_settings': {
        'OTHER_LDFLAGS': [
          '-lstdc++',
        ],
      },
      'conditions': [
        ['OS=="win"', {
          'defines': [
            'BUILD_GL32',
            'KEYWORD1=GLAPI',
            'KEYWORD2=GLAPIENTRY',
          ],
        }],
      ],
      'include_dirs': [
        'src/src/mapi',
        'src/src/mesa',
        'src/src/mesa/drivers',
        '<(generated_src_dir)/mesa',
      ],
      'msvs_disabled_warnings': [
          4005, 4018, 4065, 4090, 4099, 4273, 4291, 4345, 4267,
      ],
      'sources': [
        'src/src/mesa/drivers/common/driverfuncs.c',
        'src/src/mesa/drivers/common/driverfuncs.h',
        'src/src/mesa/drivers/common/meta.c',
        'src/src/mesa/drivers/common/meta.h',
        'src/src/mesa/drivers/osmesa/osmesa.c',
        'src/src/mesa/drivers/osmesa/osmesa.def',
      ],
    },
  ],
}
