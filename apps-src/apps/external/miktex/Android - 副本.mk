SUB_PATH := external/miktex

LOCAL_SRC_FILES +=  \
	$(SUB_PATH)/Libraries/3rd/fmt/source/src/format.cc \
	$(SUB_PATH)/Libraries/3rd/fmt/source/src/os.cc \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/autofit/autofit.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftbase.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftbbox.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftbdf.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftbitmap.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftcid.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftdebug.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftfstype.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftgasp.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftglyph.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftgxval.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftinit.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftmm.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftotval.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftpatent.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftpfr.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftstroke.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftsynth.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftsystem.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/fttype1.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/base/ftwinfnt.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/bdf/bdf.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/bzip2/ftbzip2.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/cache/ftcache.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/cff/cff.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/cid/type1cid.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/gzip/ftgzip.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/lzw/ftlzw.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/pcf/pcf.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/pfr/pfr.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/psaux/psaux.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/pshinter/pshinter.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/psnames/psnames.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/raster/raster.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/sdf/sdf.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/sfnt/sfnt.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/smooth/smooth.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/svg/svg.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/truetype/truetype.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/type1/type1.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/type42/type42.c \
	$(SUB_PATH)/Libraries/3rd/freetype2/source/src/winfonts/winfnt.c \
	$(SUB_PATH)/Libraries/3rd/getopt/source/posix/getopt.c \
	$(SUB_PATH)/Libraries/3rd/getopt/source/posix/getopt1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/assert.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/compat.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/errno.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/extract-dbl.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/fib_table.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/invalid.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/memory.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/add.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/addmul_1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/add_1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/add_err1_n.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/add_err2_n.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/add_n.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/bdiv_dbm1c.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/bdiv_q.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/bdiv_q_1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/binvert.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/cmp.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/com.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/compute_powtab.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/copyd.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/copyi.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/dcpi1_bdiv_q.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/dcpi1_bdiv_qr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/dcpi1_divappr_q.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/dcpi1_div_q.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/dcpi1_div_qr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/divexact.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/dive_1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/divrem.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/divrem_1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/divrem_2.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/div_q.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/div_qr_2n_pi1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/fib2m.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/fib2_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/gcdext.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/gcdext_1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/gcdext_lehmer.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/gcd_subdiv_step.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/get_str.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/hgcd.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/hgcd2.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/hgcd2_jacobi.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/hgcd_appr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/hgcd_jacobi.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/hgcd_matrix.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/hgcd_reduce.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/hgcd_step.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/invert.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/invertappr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/jacbase.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/jacobi.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/jacobi_2.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/lshift.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/lshiftc.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/matrix22_mul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/matrix22_mul1_inverse_vector.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mode1o.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mod_1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mod_1_1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mod_1_2.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mod_1_3.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mod_1_4.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mullo_basecase.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mullo_n.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mulmid_basecase.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mulmod_bnm1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mul_1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mul_basecase.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mul_fft.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mu_bdiv_q.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mu_divappr_q.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mu_div_q.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/mu_div_qr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/neg.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/nussbaumer_mul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/powlo.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/powm.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/pre_divrem_1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/redc_1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/redc_n.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/rshift.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sbpi1_bdiv_q.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sbpi1_bdiv_qr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sbpi1_divappr_q.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sbpi1_div_q.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sbpi1_div_qr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/scan0.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/scan1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/set_str.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sizeinbase.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sqr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sqrlo.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sqrlo_basecase.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sqrmod_bnm1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sqrtrem.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sqr_basecase.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sub.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/submul_1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sub_1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sub_err2_n.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/sub_n.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/tdiv_qr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom22_mul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom2_sqr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom32_mul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom33_mul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom3_sqr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom42_mul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom42_mulmid.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom43_mul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom44_mul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom4_sqr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom53_mul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom63_mul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom6h_mul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom6_sqr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom8h_mul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom8_sqr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom_couple_handling.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom_eval_dgr3_pm1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom_eval_dgr3_pm2.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom_eval_pm1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom_eval_pm2.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom_eval_pm2exp.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom_eval_pm2rexp.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom_interpolate_12pts.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom_interpolate_16pts.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom_interpolate_5pts.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom_interpolate_6pts.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom_interpolate_7pts.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/toom_interpolate_8pts.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpn/generic/zero_p.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/abs.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/add.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/add_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/aorsmul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/aorsmul_i.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/cdiv_q.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/cdiv_qr_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/cdiv_q_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/cfdiv_q_2exp.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/clear.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/cmp.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/cmpabs.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/cmp_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/com.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/divexact.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/fdiv_q.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/fdiv_qr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/fdiv_q_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/fdiv_r_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/fits_slong.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/gcdext.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/get_si.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/get_str.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/get_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/init.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/init2.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/invert.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/iset.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/iset_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/jacobi.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/mod.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/mul.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/mul_2exp.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/mul_si.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/mul_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/neg.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/n_pow_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/popcount.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/powm.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/powm_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/pow_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/realloc.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/realloc2.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/scan0.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/scan1.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/set.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/set_si.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/set_str.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/set_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/size.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/sizeinbase.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/sqrt.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/sqrtrem.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/sub.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/sub_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/swap.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/tdiv_q.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/tdiv_qr.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/tdiv_q_2exp.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/tdiv_r.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/tdiv_r_2exp.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/tstbit.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/ui_pow_ui.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mpz/urandomb.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mp_bases.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mp_bpl.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mp_clz_tab.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mp_dv_tab.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mp_get_fns.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mp_minv_tab.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/mp_set_fns.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/nextprime.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/popcount.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/tal-reent.c \
	$(SUB_PATH)/Libraries/3rd/gmp/source/version.c \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/call_machine.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/CmapCache.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/Code.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/Collider.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/Decompressor.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/Face.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/FeatureMap.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/FileFace.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/Font.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/GlyphCache.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/GlyphFace.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/gr_char_info.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/gr_face.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/gr_features.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/gr_font.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/gr_logging.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/gr_segment.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/gr_slot.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/Intervals.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/json.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/Justifier.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/NameTable.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/Pass.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/Position.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/Segment.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/Silf.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/Slot.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/Sparse.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/TtfUtil.cpp \
	$(SUB_PATH)/Libraries/3rd/graphite2/source/src/UtfCodec.cpp \
	$(SUB_PATH)/Libraries/3rd/harfbuzz/source/src/harfbuzz.cc \
	$(SUB_PATH)/Libraries/3rd/harfbuzz/source/src/hb-ft.cc \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lapi.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lauxlib.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lbaselib.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lbitlib.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lcode.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lcorolib.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lctype.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/ldblib.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/ldebug.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/ldo.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/ldump.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lfunc.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lgc.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/linit.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/liolib.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/llex.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lmathlib.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lmem.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/loadlib.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lobject.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lopcodes.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/loslib.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lparser.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lstate.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lstring.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lstrlib.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/ltable.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/ltablib.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/ltm.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lundump.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lutf8lib.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lvm.c \
	$(SUB_PATH)/Libraries/3rd/lua53/source/src/lzio.c \
	$(SUB_PATH)/Libraries/3rd/md5/source/md5.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/abs.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/acos.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/acosh.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/add.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/add_d.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/add_fr.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/add_q.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/add_si.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/add_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/add_z.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/alea.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/asin.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/asinh.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/atan.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/atan2.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/bisect.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/blow.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/clear.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/clears.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/cmp.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/cmp_sym_pi.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/constants.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/cos.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/diam.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/div.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/div_2exp.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/div_2si.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/div_2ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/div_ext.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/div_fr.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/div_q.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/div_si.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/div_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/div_z.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/error.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/exp.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/exp2.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/fr_div.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/fr_sub.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/get_d.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/get_endpoints.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/get_fr.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/get_prec.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/get_version.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/has_zero.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/increase.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/init.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/init2.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/inits.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/inits2.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/intersect.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/interv_d.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/interv_fr.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/interv_si.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/interv_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/interv_z.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/inv.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/is_empty.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/is_inside.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/log.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/log2.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/mag.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/mid.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/mig.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/mul.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/mul_2exp.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/mul_2si.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/mul_2ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/mul_fr.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/mul_q.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/mul_si.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/mul_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/mul_z.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/neg.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/predicates.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/put.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/put_fr.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/put_q.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/put_si.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/put_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/put_z.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/quadrant.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/q_div.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/q_sub.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/revert_if_needed.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/round_prec.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/set.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/set_d.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/set_flt.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/set_fr.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/set_prec.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/set_q.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/set_si.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/set_str.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/set_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/set_z.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/sign.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/sin.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/si_div.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/si_sub.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/sqr.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/sqrt.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/sub.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/sub_fr.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/sub_q.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/sub_si.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/sub_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/sub_z.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/swap.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/ui_div.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/ui_sub.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/union.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/urandom.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/z_div.c \
	$(SUB_PATH)/Libraries/3rd/mpfi/source/src/z_sub.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/abort_prec_max.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/acos.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/acosh.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/add.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/add1.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/add1sp.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/add_d.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/add_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/agm.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/asin.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/asinh.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/atan.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/atan2.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/cache.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/clear.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/clears.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/cmp.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/cmp2.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/cmpabs.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/cmp_si.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/cmp_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/comparisons.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/constant.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/const_catalan.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/const_euler.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/const_log2.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/const_pi.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/cos.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/div.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/div_2si.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/div_2ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/div_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/exceptions.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/exp.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/exp2.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/exp3.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/exp_2.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/extract.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/fits_sint.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/fits_slong.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/fits_ulong.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/frac.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/free_cache.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/get_d.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/get_si.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/get_str.c \>
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/get_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/get_z.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/get_z_2exp.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/gmp_op.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/init.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/init2.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/inits2.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/int_ceil_log2.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/isinteger.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/isnum.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/isqrt.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/log.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/log2.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/min_prec.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/mpfr-gmp.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/mpn_exp.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/mp_clz_tab.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/mul.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/mulders.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/mul_2si.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/mul_2ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/mul_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/nbits_ulong.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/neg.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/next.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/pool.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/powerof2.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/rem1.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/rint.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/round_near_x.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/round_p.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/round_prec.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/scale2.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/setmax.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/setmin.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_d.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_dfl_prec.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_f.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_inf.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_nan.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_prec.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_q.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_rnd.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_si.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_si_2exp.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_str.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_ui_2exp.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_z.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_zero.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/set_z_2exp.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/sgn.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/sin.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/sin_cos.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/si_op.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/sqr.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/sqrt.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/sqrt_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/strtofr.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/sub.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/sub1.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/sub1sp.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/sub_ui.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/swap.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/ubf.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/ui_div.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/ui_sub.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/urandom.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/urandomb.c \
	$(SUB_PATH)/Libraries/3rd/mpfr/source/src/version.c \
	$(SUB_PATH)/Libraries/3rd/popt/source/src/popt.c \
	$(SUB_PATH)/Libraries/3rd/popt/source/src/poptconfig.c \
	$(SUB_PATH)/Libraries/3rd/popt/source/src/popthelp.c \
	$(SUB_PATH)/Libraries/3rd/popt/source/src/poptint.c \
	$(SUB_PATH)/Libraries/3rd/popt/source/src/poptparse.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/pparray.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/ppcrypt.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/ppdict.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/ppheap.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/ppload.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/ppstream.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/ppxref.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/util/utilbasexx.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/util/utilcrypt.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/util/utilflate.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/util/utilfpred.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/util/utiliof.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/util/utillog.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/util/utillzw.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/util/utilmd5.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/util/utilmem.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/util/utilmemheap.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/util/utilmemheapiof.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/util/utilmeminfo.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/util/utilnumber.c \
	$(SUB_PATH)/Libraries/3rd/pplib/source/src/util/utilsha.c \
	$(SUB_PATH)/Libraries/3rd/regex/source/posix/regex.c \
	$(SUB_PATH)/Libraries/3rd/rose_libs/log4cxx_logger.cpp \
	$(SUB_PATH)/Libraries/3rd/teckit/source/source/Engine.cpp \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriCommon.c \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriCompare.c \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriEscape.c \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriFile.c \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriIp4.c \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriIp4Base.c \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriMemory.c \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriNormalize.c \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriNormalizeBase.c \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriParse.c \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriParseBase.c \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriQuery.c \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriRecompose.c \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriResolve.c \
	$(SUB_PATH)/Libraries/3rd/uriparser/source/src/UriShorten.c \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/fofi/FoFiBase.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/fofi/FoFiEncodings.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/fofi/FoFiIdentifier.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/fofi/FoFiTrueType.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/fofi/FoFiType1.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/fofi/FoFiType1C.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/goo/FixedPoint.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/goo/gfile.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/goo/GHash.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/goo/GList.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/goo/gmem.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/goo/gmempp.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/goo/GString.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/goo/Trace.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/AcroForm.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Annot.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Array.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/BuiltinFont.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/BuiltinFontTables.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Catalog.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/CharCodeToUnicode.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/CMap.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Decrypt.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Dict.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Error.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/FontEncodingTables.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Function.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Gfx.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/GfxFont.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/GfxState.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/GlobalParams.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/JArithmeticDecoder.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/JBIG2Stream.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/JPXStream.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Lexer.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Link.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/NameToCharCode.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Object.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/OptionalContent.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Outline.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/OutputDev.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Page.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Parser.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/PDF417Barcode.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/PDFDoc.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/PDFDocEncoding.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/PSTokenizer.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/SecurityHandler.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Stream.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/TextString.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/UnicodeMap.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/UnicodeRemapping.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/UnicodeTypeTable.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/UTF8.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/XFAScanner.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/XRef.cc \
	$(SUB_PATH)/Libraries/3rd/xpdf/source/xpdf/Zoox.cc \
	$(SUB_PATH)/Libraries/3rd/zzip/source/zzip/zzip_dir.c \
	$(SUB_PATH)/Libraries/3rd/zzip/source/zzip/err.c \
	$(SUB_PATH)/Libraries/3rd/zzip/source/zzip/fetch.c \
	$(SUB_PATH)/Libraries/3rd/zzip/source/zzip/file.c \
	$(SUB_PATH)/Libraries/3rd/zzip/source/zzip/info.c \
	$(SUB_PATH)/Libraries/3rd/zzip/source/zzip/plugin.c \
	$(SUB_PATH)/Libraries/3rd/zzip/source/zzip/stat.c \
	$(SUB_PATH)/Libraries/3rd/zzip/source/zzip/write.c \
	$(SUB_PATH)/Libraries/3rd/zzip/source/zzip/zip.c \
	$(SUB_PATH)/Libraries/MiKTeX/App/vi/Runtime.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/App/app.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/c/api.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Cfg/Cfg.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/CommandLineBuilder/unx/unxCommandLineBuilder.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/CommandLineBuilder/CommandLineBuilder.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/CsvList/CsvList.cpp	\
	$(SUB_PATH)/Libraries/MiKTeX/Core/Directory/rose/roseDirectory.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Directory/Directory.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/DirectoryLister/rose/roseDirectoryLister.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/DirectoryLister/DirectoryLister.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/File/rose/roseFile.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/File/File.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/FileSystemWatcher/unx/unxFileSystemWatcher.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/FileSystemWatcher/FileSystemWatcher.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/FileSystemWatcher/FileSystemWatcherBase.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Fndb/FileNameDatabase.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Fndb/Fndb.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Fndb/makefndb.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/LockFile/LockFile.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/MD5\MD5.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/MemoryMappedFile/rose/roseMemoryMappedFile.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/MemoryMappedFile/MemoryMappedFile.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Process/Process.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/rose/roseStartupConfig.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/unx/runsh.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/unx/unxSession.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/appnames.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/config.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/error.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/files.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/filetypes.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/findfile.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/fontinfo.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/formats.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/graphics.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/gsinfo.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/init.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/languages.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/mfmodes.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/miktex.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/papersize.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/runexe.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/rungs.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/runperl.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/searchpath.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/StartupConfig.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Session/texmfroot.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Stream/BZip2Stream.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Stream/FileStream.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Stream/GzipStream.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Stream/Stream.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Stream/StreamReader.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Stream/StreamWriter.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/TemporaryDirectory/TemporaryDirectory.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/TemporaryFile/TemporaryFile.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Uri/Uri.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Utils/unx/unxUtils.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Utils/uncompress.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Utils/Utils.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/VersionNumber/VersionNumber.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/vi/Runtime.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/unx/unx.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Debug.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Core/Exceptions.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/etc/wrapper.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Extractor/vi/Runtime.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Extractor/Extractor.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/KPathSeaEmulation/kpsemu.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Locale/Translator.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/PackageManager/vi/Runtime.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/PackageManager/ComboCfg.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/PackageManager/CurlWebFile.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/PackageManager/CurlWebSession.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/PackageManager/ExpatTpmParser.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/PackageManager/PackageDataStore.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/PackageManager/PackageInstallerImpl.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/PackageManager/PackageIteratorImpl.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/PackageManager/PackageManagerImpl.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/PackageManager/PackageRepositoryDataStore.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/PackageManager/RemoteService.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/PackageManager/RestRemoteService.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/PackageManager/TpmParser.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/PackageManager/WebSession.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Resources/ResourceRepository.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Setup/unx/unxSetupService.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Setup/LogFile.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Setup/SetupService.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/TeXAndFriends\c4plib.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/TeXAndFriends\c4pstart.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/TeXAndFriends\etexapp.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/TeXAndFriends\inputline.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/TeXAndFriends\mfapp.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/TeXAndFriends\texapp.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/TeXAndFriends\texmfapp.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/TeXAndFriends\texmflib.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/TeXAndFriends\webapp.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Trace/StopWatch.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Trace/TraceStream.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Util/PathName/rose/rosePathName.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Util/PathName/PathName.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Util/rose/roseHelpers.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Util/Helpers.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Util/PathNameParser.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Util/PathNameUtil.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Util/StringUtil.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Util/Tokenizer.cpp \
	$(SUB_PATH)/Libraries/MiKTeX/Web2CEmulation/w2cemu.cpp \	
	$(SUB_PATH)/Programs/MiKTeX/makex/makefmt.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/shims/mkfntmap.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/shims/mktexlsr.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/shims/texlinks.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/shims/updmap.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/filesystem/commands/watch.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/filesystem/topic.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/fndb/commands/refresh.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/fndb/commands/remove.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/fndb/topic.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/fontmaps/commands/configure.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/fontmaps/commands/FontMapManager.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/fontmaps/commands/set-option.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/fontmaps/commands/show-option.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/fontmaps/topic.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/formats/commands/build.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/formats/commands/FormatsManager.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/formats/commands/list.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/formats/topic.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/languages/commands/configure.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/languages/commands/list.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/languages/topic.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/Links/commands/install.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/Links/commands/LinksManager.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/Links/commands/list.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/Links/commands/uninstall.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/Links/topic.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/packages/commands/checkupdate.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/packages/commands/checkupgrade.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/packages/commands/info.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/packages/commands/install.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/packages/commands/list.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/packages/commands/private.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/packages/commands/remove.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/packages/commands/require.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/packages/commands/update.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/packages/commands/updatepackagedatabase.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/packages/commands/upgrade.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/packages/commands/verify.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/packages/topic.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/repositories/commands/checkbandwidth.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/repositories/commands/info.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/repositories/commands/list.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/repositories/commands/private.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/repositories/topic.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/Command.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/topics/Topic.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/internal.cpp \
	$(SUB_PATH)/Programs/MiKTeX/miktex/miktex.cpp \
	
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\luainit-hb.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\luastuff-hb.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\miktex\miktex.cpp">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\printing-hb.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\dvi\dvigen.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\dofont.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\luafont.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\mapfile.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\pkin.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\sfnt.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\texfont.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\tfmofm.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\tounicode.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\tt_glyf.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\tt_table.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\vfovf.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\vfpacket.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\writecff.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\writeenc.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\writefont.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\writet1.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\writet3.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\writettf.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\writetype0.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\font\writetype2.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\image\pdftoepdf.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\image\writeimg.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\image\writejbig2.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\image\writejp2.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\image\writejpg.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\image\writepng.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lang\hnjalloc.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lang\hyphen.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lang\texlang.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaffi\call.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaffi\ctype.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaffi\ffi.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaffi\parser.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafilesystem\src\lfs.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\autohint.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\clipnoui.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\cvundoes.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\dumppfa.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\encoding.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\featurefile.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\fontviewbase.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\fvcomposit.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\fvfonts.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\lookups.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\macbinary.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\macenc.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\mathconstants.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\memory.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\mm.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\namelist.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\noprefs.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\nouiutil.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\parsepfa.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\parsettf.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\parsettfatt.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\psread.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\pua.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\python.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\sfd1.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\splinechar.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\splinefill.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\splinefont.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\splineorder2.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\splineoverlap.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\splinerefigure.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\splinesave.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\splinesaveafm.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\splinestroke.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\splineutil.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\splineutil2.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\start.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\stemdb.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\tottf.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\tottfgpos.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\fontforge\ttfspecial.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\fontforge\gutils\fsys.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\src\ffdummies.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luafontloader\src\luafflib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaharfbuzz\src\luaharfbuzz\blob.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaharfbuzz\src\luaharfbuzz\buffer.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaharfbuzz\src\luaharfbuzz\class_utils.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaharfbuzz\src\luaharfbuzz\direction.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaharfbuzz\src\luaharfbuzz\face.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaharfbuzz\src\luaharfbuzz\feature.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaharfbuzz\src\luaharfbuzz\font.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaharfbuzz\src\luaharfbuzz\language.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaharfbuzz\src\luaharfbuzz\luaharfbuzz.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaharfbuzz\src\luaharfbuzz\ot.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaharfbuzz\src\luaharfbuzz\script.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaharfbuzz\src\luaharfbuzz\tag.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaharfbuzz\src\luaharfbuzz\unicode.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luaharfbuzz\src\luaharfbuzz\variation.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luamd5\md5.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luamd5\md5lib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luapeg\lpeg.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\auxiliar.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\buffer.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\compat.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\except.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\inet.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\io.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\luasocket.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\lua_preload.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\mime.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\options.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\select.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\serial.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\socket.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\tcp.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\timeout.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luasocket\src\udp.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luatex.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luazip\src\luazip.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luazlib\lgzip.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\luazlib\lzlib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\helpers.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\lcallbacklib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\lfontlib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\limglib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\liolibext.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\lkpselib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\llanglib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\llualib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\lnewtokenlib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\lnodelib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\loslibext.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\lpdfelib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\lpdflib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\lpdfscannerlib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\lstatslib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\lstrlibext.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\ltexiolib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\ltexlib.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\luanode.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\luatex-core.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\luatoken.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\lua\texluac.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfaction.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfannot.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfcolorstack.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfdest.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdffont.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfgen.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfglyph.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfimage.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdflink.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdflistout.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfliteral.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfobj.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfoutline.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfpage.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfpagetree.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfrule.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfsaverestore.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfsetmatrix.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfshipout.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdftables.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfthread.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\pdf\pdfxform.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\slnunicode\slnunico.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\align.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\arithmetic.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\backend.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\buildpage.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\commands.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\conditional.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\directions.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\dumpdata.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\equivalents.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\errors.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\expand.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\extensions.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\filename.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\inputstack.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\linebreak.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\mainbody.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\maincontrol.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\mathcodes.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\memoryword.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\mlist.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\nesting.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\packaging.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\postlinebreak.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\primitive.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\scanning.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\stringpool.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\texdeffont.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\texfileio.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\texmath.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\texnodes.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\textcodes.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\tex\textoken.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\unilib\alphabet.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\unilib\ArabicForms.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\unilib\char.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\unilib\cjk.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\unilib\gwwiconv.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\unilib\ucharmap.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\unilib\unialt.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\unilib\usprintf.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\unilib\ustring.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\unilib\utype.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\utils\avl.c">
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\utils\avlstuff.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\utils\managed-sa.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\source\utils\unistring.c" />
    <ClCompile Include="..\..\external\miktex\Programs\TeXAndFriends\luatex\utils-hb.c" />

	
	$(SUB_PATH)/Programs/TeXAndFriends/synctex/source/synctex.cpp \
