#!/bin/bash

# Multipurpose script for master->stl1-branch code checkout. What it
# does depends on the command (first argument) passed into it:
#   * checkout: checkout files from master into stl1-branch,
#     limited to certain directories and regexes. Doesn't actually
#     switch to stl1-branch, you have to do that.
#   * diff: show differences between stl1-branch and master in files that
#     should be the same between the two.
#   * showSliced: show files that differ between stl1-branch and master and
#     that were in commits that had files that were checked-out into master.
#     So-called 'sliced' because they were part of a single commit that was
#     'sliced' apart.
set -u

FromBranch=origin/master
ToBranch=origin/stl1-branch
DirList="Esm/ FabricSim/ IbAccess/ IbaTools/ IbPrint/ Log/ Makerules/ opamgt/ OpenIb_Host/ Sim/ Topology/ Xml/"

# Limit checkout of master code to a) C/C++ files and b) non-C/C++
# files where necessary to get code to build (basically just Makefiles)
inclRegex='\.([ch]|cpp|hpp)|Esm/ib/src/(eeph|eepha|eeph_mux|em)/|IbaTools/opaxlattopology/opaxlattopology.sh'
exclRegex="build_label"
function getFilesToCheckout() {
	git ls-tree -r --name-only $FromBranch -- $DirList | egrep $inclRegex | egrep -v $exclRegex
}

# When testing for sliced files (files that were in a commit that 
slicedExclRegex=""
for d in $DirList ; do
	if [[ -n $slicedExclRegex ]] ; then
		slicedExclRegex+="|"
	fi
	slicedExclRegex+=$d
done

function getExclFilesForCmt() {
	local cmt=$1 ; shift
	git diff-tree --no-commit-id --name-only -r $cmt | egrep $inclRegex | egrep -v $slicedExclRegex
}

cmd=$1; shift

if [[ $cmd = "checkout" ]] ; then
	getFilesToCheckout | egrep $inclRegex | egrep -v $exclRegex | while read f ; do
		git checkout $FromBranch -- $f
	done
elif [[ $cmd = "diff" ]] ; then
	diffOpts=$*
	getFilesToCheckout | xargs git diff $ToBranch $FromBranch $diffOpts --
elif [[ $cmd = "showSliced" ]] ; then
	# TODO get 'show files' and 'show commit' options from commandline
	# TODO also get additional exclude regexes from command line
	showFiles=1
	showCmt=0

	getFilesToCheckout | xargs git rev-list ${ToBranch}..${FromBranch} -- | while read cmt ; do
		slicedFiles=`getExclFilesForCmt $cmt`
		if [[ -z $slicedFiles ]] ; then
			continue
		fi

		# No actual diffs between the two branches concerning files that weren't checked out
		# from $FromBranch into $ToBranch - skip this commit
		if git diff --quiet $ToBranch $FromBranch -- $slicedFiles ; then
			continue
		fi

		if [[ $showCmt -ne 0 ]] ; then
			git show --quiet $cmt
		fi

		if [[ $showFiles -ne 0 ]] ; then
			# This will include duplicates; pipe into sort | uniq to get unique file list
			git diff-tree --no-commit-id --name-only -r $cmt | egrep $inclRegex | \
				egrep -v $slicedExclRegex | while read file ; do

				# Only show files from this commit if they actually
				# differ between $FromBranch and $ToBranch
				if ! git diff --quiet $ToBranch $FromBranch -- $file ; then
					echo $file
				fi
			done
		fi

		# Neither $showCmt nor $showFiles, just print commit SHA
		if [[ $showCmt -eq 0 && $showFiles -eq 0 ]] ; then
			echo $cmt
		fi
	done
else
	echo "Error: unrecognized command \"$cmd\"" >&2
	echo "Usage: $0 (checkout|diff [<diff options>]|showSliced [<showSliced options>])" >&2
	exit 1
fi
