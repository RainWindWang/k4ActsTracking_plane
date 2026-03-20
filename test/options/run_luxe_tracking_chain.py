#!/usr/bin/env python3

from Gaudi.Configuration import VERBOSE, DEBUG, INFO
from k4FWCore import ApplicationMgr, IOSvc
from k4FWCore.parseArgs import parser

from Configurables import GeoSvc, EventDataSvc
from Configurables import ActsGeoGen3PlaneSvc, TrackerMappingSvc

from Configurables import DigitizationSvc
from Configurables import DigiAlg, TrackerDigitizerTool

from Configurables import TrackerEDMToActsAlg
from Configurables import SeedingAlg
from Configurables import TrackFindingCKFAlg
from Configurables import TrackFittingAlg
from Configurables import ActsToEdm4hepTrackWriterAlg

# -----------------

parser.add_argument("--compactFile", help="Compact file")
parser.add_argument("--inputFile", default="positrons_1_edm4hep.root", help="Input EDM4hep ROOT")
parser.add_argument("--outputFile", default="luxe_tracks_fitted_edm4hep.root", help="Output EDM4hep ROOT")
args = parser.parse_known_args()[0]

# -----------------
# DD4hep geometry
geoSvc = GeoSvc()
geoSvc.detectors = [args.compactFile]
geoSvc.OutputLevel = INFO

# -----------------
# ACTS Gen3 plane geometry
actsGeoPlaneSvc = ActsGeoGen3PlaneSvc("ActsGeoPlaneSvc")
actsGeoPlaneSvc.DetElementName   = "Tracker"
actsGeoPlaneSvc.LayerPatternExpr = r"layer\d"
actsGeoPlaneSvc.OutputLevel      = DEBUG

# -----------------
# Mapping: CellID -> Acts::Surface + GeometryId
mappingSvc = TrackerMappingSvc("TrackerMappingSvc")
mappingSvc.ActsGeoSvc   = "ActsGeoPlaneSvc"
mappingSvc.OutputLevel  = DEBUG

# -----------------
# Digitization infrastructure service
digiSvc = DigitizationSvc("DigitizationSvc")
digiSvc.OutputLevel = INFO

# -----------------
# I/O
iosvc = IOSvc()
iosvc.Input       = args.inputFile
iosvc.Output      = args.outputFile
iosvc.OutputLevel = INFO

# ============================================================
# 1) Digitization stage
# ============================================================

# Tracker digitizer tool (SimTrackerHit -> TrackerHitPlane)
trackerDigiTool = TrackerDigitizerTool("TrackerDigitizerTool")
trackerDigiTool.ActsGeoSvc     = "ActsGeoPlaneSvc"
trackerDigiTool.MappingSvc     = "TrackerMappingSvc"
trackerDigiTool.DigitizationSvc = "DigitizationSvc"
trackerDigiTool.OutputLevel    = DEBUG

digiAlg = DigiAlg("DigiAlg")
digiAlg.InputCollection  = "SiHits"           # edm4hep::SimTrackerHit
digiAlg.OutputCollection = "DigiTrackerHits"  # edm4hep::TrackerHitPlane
digiAlg.Tools = [trackerDigiTool]
digiAlg.OutputLevel = INFO

# ============================================================
# 2) EDM -> ACTS
# ============================================================

convAlg = TrackerEDMToActsAlg("TrackerEDMToActsAlg")
convAlg.ActsGeoSvc      = "ActsGeoPlaneSvc"
convAlg.MappingSvc      = "TrackerMappingSvc"

convAlg.InputHits       = "DigiTrackerHits"     # edm4hep::TrackerHitPlaneCollection
convAlg.OutputSpacePoints = "TrackerSpacePoints"
convAlg.OutputSourceLinks = "TrackerSourceLinks"
convAlg.OutputMeasurements = "TrackerMeasurements"
convAlg.OutputLevel = INFO

# ============================================================
# 3) Seeding
# ============================================================

seedAlg = SeedingAlg("SeedingAlg")
seedAlg.ActsGeoSvc        = "ActsGeoPlaneSvc"
seedAlg.InputSpacePoints  = "TrackerSpacePoints"
seedAlg.InputSourceLinks  = "TrackerSourceLinks"
seedAlg.OutputSeeds       = "TrackerSeeds"
# seedAlg.OutputInitialTrackParameters = "InitialTrackParameters"
seedAlg.OutputLevel       = INFO

# ============================================================
# 4) Track finding (CKF)
# ============================================================

ckfAlg = TrackFindingCKFAlg("TrackFindingCKFAlg")
ckfAlg.ActsGeoSvc         = "ActsGeoPlaneSvc"
ckfAlg.InputSeeds         = "TrackerSeeds"
ckfAlg.InputSourceLinks   = "TrackerSourceLinks"
ckfAlg.InputMeasurements  = "TrackerMeasurements"
ckfAlg.OutputTrackCandidates = "ActsTrackCandidates"
ckfAlg.OutputLevel        = INFO

# ============================================================
# 5) Track fitting
# ============================================================

fitAlg = TrackFittingAlg("TrackFittingAlg")
fitAlg.ActsGeoSvc         = "ActsGeoPlaneSvc"
fitAlg.InputTrackCandidates = "ActsTrackCandidates"
fitAlg.InputSourceLinks   = "TrackerSourceLinks"
fitAlg.InputMeasurements  = "TrackerMeasurements"
fitAlg.OutputFittedTracks = "FittedActsTracks"
fitAlg.OutputLevel        = INFO

# ============================================================
# 6) ACTS -> edm4hep writer (final EDM output)
# ============================================================

writerAlg = ActsToEdm4hepTrackWriterAlg("ActsToEdm4hepTrackWriterAlg")
writerAlg.ActsGeoSvc      = "ActsGeoPlaneSvc"
writerAlg.InputFittedTracks = "FittedActsTracks"
#writerAlg.InputDigiHits     = "DigiTrackerHits"
writerAlg.OutputTracks      = "ReconstructedTracks"
writerAlg.OutputLevel       = INFO
# writerAlg.OutputTrackStates = "ReconstructedTrackStates"

# -----------------
# AppMgr
ApplicationMgr(
    TopAlg=[digiAlg,convAlg,seedAlg,ckfAlg,fitAlg,writerAlg,],
    ExtSvc=[geoSvc,actsGeoPlaneSvc,mappingSvc,digiSvc,EventDataSvc(),iosvc,],
    EvtMax=1,
    EvtSel="NONE",
)
