\name{varCovar}
\alias{varCovar}
\title{Function to create a changing covariate object.}
\description{
  This function creates a changing covariate object from a matrix.
}
\usage{
varCovar(val, nodeSet='Actors')
}
\arguments{
  \item{val}{ Matrix of covariate values, one row for each actor, one
    column for each period. }
  \item{nodeSet}{Character string containing the name of the associated
    node set. If the entire data set contains more than one node set,
    then the node sets must be specified in all data objects.}
}
\details{
  When part of a Siena data object, the covariate is assumed to be
  associated with node set nodeSet of the Siena data object.
}
\value{
  Returns the covariate as an object of class 'varCovar', in which
  form it can be used as an argument to SienaData.create.
}
\references{See \url{http://www.stats.ox.ac.uk/~snijders/siena/}}
\author{Ruth Ripley}
\seealso{\code{\link{sienaDataCreate}}, \code{\link{coCovar}},
\code{\link{coDyadCovar}}, \code{\link{varDyadCovar}}}
\examples{

myvarCovar <- varCovar(s50a)

}
\keyword{classes}
