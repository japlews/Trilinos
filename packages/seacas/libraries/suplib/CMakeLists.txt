
TRIBITS_SUBPACKAGE(Suplib)
INCLUDE(FortranSettings)

SET(SUPLIB_VERSION_MAJOR "3")
SET(SUPLIB_VERSION_MINOR "08")
SET(SUPLIB_VERSION_PATCH "0")
SET(SUPLIB_VERSION "${SUPLIB_VERSION_MAJOR}.${SUPLIB_VERSION_MINOR}")
SET(SUPLIB_VERSION_FULL "${SUPLIB_VERSION}.${SUPLIB_VERSION_PATCH}")

TRIBITS_INCLUDE_DIRECTORIES(
  "${CMAKE_CURRENT_SOURCE_DIR}"
  )

SET(SOURCES
  abrstr.f
  copint.f
  dbpini.f
  ffaddv.f
  ffrrng.f
  indexi.f
  intadd.f
  logerr.f
  numeql.f
  prterr.f
  wrapup.f
  addv.f
  cpuifc.f
  dbpqa.f
  ffchar.f
  ffvrng.f
  indexn.f
  intstr.f
  lowstr.f
  numstr.f
  numstr1.f
  ran1.f
  argparse.F
  cpyint.f
  dbvini.f
  ffexst.f
  indexr.f
  iowdsz.f
  matstr.f
  okabrt.f
  scneof.f
  banner.f
  cpylog.f
  engnot.f
  ffintg.f
  gslibv.f
  indexx.f
  ismax.f
  memerr.f
  opnfil.f
  screrr.f
  bannr2.f
  cpyrea.f
  engstr.f
  ffmatc.f
  heap_sort.f
  inigrd.f
  ismin.f
  minmax.f
  packt.f
  shocmd.f
  batch.f
  cpyrgt.f
  ffaddc.f
  ffneed.f
  help.f
  iniint.f
  lenstr.f
  minmxl.f
  pcklab.f
  sqzstr.f
  ckexod.f
  cpystr.f
  ffaddi.f
  ffnumb.f
  hunt.f
  inilog.f
  locint.f
  minmxs.f
  pckstr.f
  strtup.f
  cknone.f
  dberr.f
  ffaddo.f
  ffonof.f
  idblnk.f
  inirea.f
  locrea.f
  num2ix.f
  pltond.f
  subv.f
  ckwhol.f
  dbiv0.f
  ffaddr.f
  ffreal.f
  iengrx.f
  inistr.f
  locstr.f
  numeqi.f
  pltone.f
  wheneq.f
  addlog.c
  )

TRIBITS_ADD_LIBRARY(
  suplib
  NOINSTALLHEADERS
  SOURCES ${SOURCES}
  )

TRIBITS_SUBPACKAGE_POSTPROCESS()
