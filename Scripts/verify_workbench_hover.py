"""Compare real menu captures to verify stronger hover shading, without altering images."""
from pathlib import Path
import argparse,json
from PIL import Image,ImageStat
parser=argparse.ArgumentParser();parser.add_argument('directory');parser.add_argument('--output',required=True);args=parser.parse_args()
root=Path(args.directory);normal=Image.open(root/'Workbench-Deck.png').convert('RGB');card=Image.open(root/'Workbench-Card-Hover.png').convert('RGB');rider=Image.open(root/'Workbench-Rider-Hover.png').convert('RGB')
assert normal.size==card.size==rider.size
scale=normal.width/1600

def mean(image,box):
 return ImageStat.Stat(image.crop(tuple(round(value*scale) for value in box))).mean

def ratio(image,box):
 return sum(mean(image,box))/sum(mean(normal,box))
shading=ratio(card,(530,265,700,345));unaffected=ratio(card,(800,265,950,345))
before=mean(normal,(370,185,405,206));after=mean(rider,(370,185,405,206))
assert .55<shading<.9,('Card artwork did not visibly darken on actual pointer hover',shading)
assert .98<unaffected<1.02,('Hover changed another card',unaffected)
assert after[1]>before[1]*1.15,('Rider row did not receive the stronger colored hover fill',before,after)
report={'result':'WORKBENCH_HOVER_PASS','resolution':list(normal.size),'hoveredCardBrightnessRatio':shading,'otherCardBrightnessRatio':unaffected,'riderBackgroundBefore':before,'riderBackgroundAfter':after}
output=Path(args.output);output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2));print(json.dumps(report))
