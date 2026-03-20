#!/usr/bin/env python3

from Gaudi.Configuration import DEBUG, INFO

from Configurables import (
    GeoSvc,
    EventDataSvc,
    ActsGeoGen3PlaneSvc,
    TrackerMappingSvc,
    DigitizationSvc,
    DigiAlg,
    TrackerEDMToActsAlg,
    SeedingAlg,
    SeedingTool,
)

from k4FWCore import ApplicationMgr, IOSvc
from k4FWCore.parseArgs import parser

parser.add_argument("--compactFile", help="DD4hep compact xml")
parser.add_argument("--inputFile", default="positrons_3_edm4hep.root",
                    help="Input EDM4hep file")
args = parser.parse_known_args()[0]

# ------------------------------------------------------------------
# Geometry / mapping
# ------------------------------------------------------------------

geoSvc = GeoSvc()
geoSvc.detectors = [args.compactFile]

actsGeoPlaneSvc = ActsGeoGen3PlaneSvc("ActsGeoPlaneSvc")
actsGeoPlaneSvc.DetElementName = "Tracker"
actsGeoPlaneSvc.LayerPatternExpr = r"layer\d"
actsGeoPlaneSvc.OutputLevel = DEBUG

mappingSvc = TrackerMappingSvc("TrackerMappingSvc")
mappingSvc.ActsGeoSvc = "ActsGeoPlaneSvc"
mappingSvc.OutputLevel = DEBUG

# ------------------------------------------------------------------
# I/O
# ------------------------------------------------------------------

iosvc = IOSvc()
iosvc.Input = args.inputFile
iosvc.Output = "digi_seeding_test.root"
iosvc.OutputLevel = INFO

# ------------------------------------------------------------------
# Digitization service
# ------------------------------------------------------------------

digiSvc = DigitizationSvc("DigitizationSvc")
digiSvc.OutputLevel = DEBUG

# Match your EDM4hep/DD4hep cellID encoding
digiSvc.IDLayout = "system:1,side:1,layer:2,module:1,sensor:5,x:32:-16,y:-16"
# Pixel pitch [mm]
digiSvc.PitchX = 0.02
digiSvc.PitchY = 0.02
# Local coordinate offset [mm]
digiSvc.OffsetX = 0.0
digiSvc.OffsetY = 0.0
# Smearing, 0 for debug
digiSvc.SmearSigmaU = 0.0
digiSvc.SmearSigmaV = 0.0

# ------------------------------------------------------------------
# Digi: SimTrackerHit -> TrackerHitPlane
# ------------------------------------------------------------------

digi = DigiAlg("DigiAlg")
digi.InputCollection = "SiHits"
digi.OutputCollection = "DigiTrackerHits"
digi.Tools = ["TrackerDigitizerTool"]
digi.OutputLevel = DEBUG

# ------------------------------------------------------------------
# EDM -> ACTS
# ------------------------------------------------------------------

edm2acts = TrackerEDMToActsAlg("TrackerEDMToActsAlg")
edm2acts.InputHits = "DigiTrackerHits"
edm2acts.OutputSpacePoints = "TrackerSpacePoints"
edm2acts.OutputSourceLinks = "TrackerSourceLinks"
edm2acts.OutputMeasurements = "TrackerMeasurements"
edm2acts.Tool = "TrackerEDMConverterTool"
edm2acts.OutputLevel = DEBUG

# ------------------------------------------------------------------
# Seeding
# ------------------------------------------------------------------

seedTool = SeedingTool("SeedingTool")
seedTool.OutputLevel = DEBUG
seedTool.enableStraightLineCut = True
seedTool.maxCollinearity = 0.01

seeding = SeedingAlg("SeedingAlg")
seeding.InputSpacePoints = "TrackerSpacePoints"
seeding.OutputSeeds = "TrackerSeeds"
seeding.SeedingTool = seedTool
seeding.Verbose = True
seeding.OutputLevel = DEBUG

#seeding = SeedingAlg("SeedingAlg")
#seeding.InputSpacePoints = "TrackerSpacePoints"
#seeding.OutputSeeds = "TrackerSeeds"
#seeding.SeedingTool = "SeedingTool"
#seeding.Verbose = True
#seeding.OutputLevel = DEBUG

ApplicationMgr(
    TopAlg=[digi,edm2acts,seeding,],
    ExtSvc=[geoSvc,actsGeoPlaneSvc,mappingSvc,digiSvc,EventDataSvc(),iosvc,],
    EvtMax=1,
    EvtSel="NONE",
)
