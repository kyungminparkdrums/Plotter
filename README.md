# How to Run

simply type
```
make
./Plotter config/<YOUR CONFIG FILE>
```

The plotter comes with options that can be added at runtime similar to bash options.  To list all of the option and details about them, simply type

`./Plotter -help`

The option are what allow the plotter to configure which graph with go on the bottom of the canvas.  You can set it to Ratio Plot (Default), different significance plots, or remove the bottom graph all together.

For more details, go to the Wiki for this code (https://github.com/BSM3G/Plotter/wiki)



if you want to youse the python plotter install:

http://www.rootpy.org/install.html

and for madplotlib plots you might want CMSSW_8_4+


# saveAsPdf
Reads the output file in root format and save certain figures inside the root file as pdf, following the options given by user
  - **`saveFigures.py`**: Get a plotter output root file and a config file that contains the options for saving the figures from the plotter output. Then, save them as pdf files following the user options given in the config file.
      ```
         python saveFigures.py --config <config file> --input_file <input root file (output file from the Plotter)>
      ```
  - Running the line above will give you the certain histograms after certain cut step that you specified in the config file, in pdf format under the name of `<cut_step>_<histogram_name>.pdf`. Unless you specified the output pdf directory in the config file, i.e. `--output_dir <dir>`, these will be stored in `./output/` directory which will be automatically created.
  - **config**: i.e. `config/saveOptions.config`
    - Config file contains options for saving figures.
    - Mandatory options to be included in every line are as follows.
      ```
         --step <cut step> --kinematic <kinematic>
      ```
    - Additional options you can add are as follows.
      ```
         --year <year (to correct the lumi on the top right if necessary)>
         --isPreliminary <True (change Work in Progress to Preliminary) or False (keep Work in Progress)>
         --output_dir <output directory>
         --legend_column <# of columns for the legend>
         --display_title <True or False>
         --draw_without_ratio <True or False>
         --set_logX <True or False>
         --x_range <x axis range in list format>
         --x_title <x axis title in TLatex format>
         --set_logY <True or False>
         --histo_y_range <stacked histogram y axis range in list format>
         --histo_y_title <stacked histogram y axis title>
         --ratio_y_range <ratio plot y axis range in list format>
      ```

    - IMPORTANT: when giving options for axis title, please note the followings (otherwise, it'll crash):
      - 1. if the title contains brackets, please put a backslash before opening and closing of the brackets, i.e. `p_{T}\(jj\)` instead of `p_{T}(jj)`
      - 2. if the title contains greek letters that require backslash in latex form, replace the backslash with hashtag, i.e. `#mu` instead of `\mu`
      ```
      # Example
      --step NDiMuonCombinations --kinematic DiMuonPt --x_range [0,300] --set_logY True --ratio_y_range [0.6,1.4] --legend_column 2 --year 2017
      --step NRecoBJet --kinematic Met --x_title p^{miss}_{T}[GeV] --histo_y_title Events --legend_column 1 --x_range [250,1000] --year 2017
      ```

```
# Example
cd ./saveAsPdf/
python saveFigures.py --config ./config/saveOptions.config --input_file <Root file>

```
