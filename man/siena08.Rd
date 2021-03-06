\name{siena08}
\alias{siena08}
\title{
Function to perform a meta analysis of a collection of Siena fits.
}
\description{
Estimates a meta analysis based on a collection of Siena fits.
}
\usage{
siena08(..., projname = "sienaMeta", bound = 5, alpha = 0.05, maxit=20)
}
\arguments{
  \item{\dots}{names of Siena fit objects, returned from
	\code{\link{siena07}}. They will be renamed if entered in format
	\code{newname=oldname}.
}
\item{projname}{
  Base name of report file if required
}
\item{bound}{
  Upper limit of standard error for inclusion in the meta analysis.
}
\item{alpha}{
  1 minus confidence level of confidence intervals.
}
\item{maxit}{
  Number of iterations of iterated least squares procedure.
}
}
\details{
  A meta analysis is performed as described
  in the Siena manual, section `Meta-analysis of Siena results'. This
  consists of three parts: an iterated weighted least squares
  modification of the method described in the reference below;
  maximum likelihood estimates and confidence intervals based on
  profile likelihoods under normality assumptions;
  and Fisher combinations of left-sided and right-sided p-values.
  These are produced for all effects separately.

  Note that the corresponding effects must have the same effect name in each
  model fit. This implies that at least covariates and behavior variables must
  have the same name in each model fit.
}
\value{An object of class \code{\link{sienaMeta}}.  There are \code{print},
  \code{summary} and \code{plot} methods for this class,

  An object of class \code{sienaMeta} is a list containing at least the
  following. (Items \code{cor.est} to
  \code{ns} appear once for each effect.)
  \item{cor.est}{Spearman rank correlation coefficient between estimates
                 and their standard errors.}
  \item{cor.pval}{p-value for above}
  \item{regfit}{Part of the result of the fit of \code{\link{iwlsm}}.}
  \item{regsummary}{The summary of the fit, which includes the
                    coefficient table.}
  \item{Tsq}{test statistic for effect zero in every model}
  \item{pTsq}{p-value for above}
  \item{tratio}{test statistics that mean effect is 0}
  \item{ptratio}{p-value for above}
  \item{Qstat}{Test statistic for variance of effects is zero}
  \item{pttilde}{p-value for above}
  \item{cjplus}{Test statistic for at least one theta strictly greater than 0}
  \item{cjminus}{Test statistic for at least one theta strictly less than 0}
  \item{cjplusp}{p-value for \code{cjplus}}
  \item{cjminusp}{p-value for \code{cjminus}}
  \item{mu.ml}{ML estimate of population mean}
  \item{mu.ml.se}{standard error of ML estimate of population mean}
  \item{sigma.ml}{ML estimate of population standard deviation}
  \item{mu.confint}{confidence interval for population mean
                    based on profile likelihood}
  \item{sigma.confint}{confidence interval for population standard deviation
                    based on profile likelihood}
  \item{n1}{Number of fits on which the meta analysis is based}
  \item{scoreplus}{Test statistic for combination of right one-sided
  p-values from score tests}
  \item{scoreminus}{Test statistic for combination of left one-sided
  p-values from score tests}
  \item{scoreplusp}{p-value for \code{scoreplus}}
  \item{scoreminusp}{p-value for \code{scoreminus}}
  \item{ns}{Number of fits on which the score test analysis is based}
  \item{thetadf}{Data frame containing the coefficients, standard errors
    and score test results}
  \item{projname}{Name for any output file to be produced by the print method}
  \item{bound}{Estimates with standard error above this value were
    excluded from the calculations}
  \item{scores}{Object of class \code{by} indicating, for each effect in
    the models, whether score test  information was present.}
}
\references{
  T. A. B. Snijders and Chris Baerveldt.
  Multilevel network study of the effects
  of delinquent behavior on friendship
  evolution. \emph{Journal of Mathematical Sociology}, 27: 123--151, 2003.

  See also the manual and \url{http://www.stats.ox.ac.uk/~snijders/siena/}
}
\author{Ruth Ripley, Tom Snijders}

\seealso{\code{\link{sienaMeta}}, \code{\link{iwlsm}}, \code{\link{siena07}}
}
\examples{
\dontrun{
# A meta-analysis for three groups does not make much sense
# for generalizing to a population of networks,
# but it the Fisher combinations of p-values are meaningful.
# But using three groups shows the idea.

Group1 <- sienaDependent(array(c(N3401, HN3401), dim=c(45, 45, 2)))
Group3 <- sienaDependent(array(c(N3403, HN3403), dim=c(37, 37, 2)))
Group4 <- sienaDependent(array(c(N3404, HN3404), dim=c(33, 33, 2)))
dataset.1 <- sienaDataCreate(Friends = Group1)
dataset.3 <- sienaDataCreate(Friends = Group3)
dataset.4 <- sienaDataCreate(Friends = Group4)
OneAlgorithm <- sienaAlgorithmCreate(projname = 'SingleGroups')
effects.1 <- getEffects(dataset.1)
effects.3 <- getEffects(dataset.3)
effects.4 <- getEffects(dataset.4)
effects.1 <- includeEffects(effects.1, transTrip)
effects.1 <- setEffect(effects.1, cycle3, fix=TRUE, test=TRUE)
effects.3 <- includeEffects(effects.3, transTrip)
effects.3 <- setEffect(effects.3, cycle3, fix=TRUE, test=TRUE)
effects.4 <- includeEffects(effects.4, transTrip)
effects.4 <- setEffect(effects.4, cycle3, fix=TRUE, test=TRUE)
ans.1 <- siena07(OneAlgorithm, data=dataset.1, effects=effects.1, batch=TRUE)
ans.3 <- siena07(OneAlgorithm, data=dataset.3, effects=effects.3, batch=TRUE)
ans.4 <- siena07(OneAlgorithm, data=dataset.4, effects=effects.4, batch=TRUE)
ans.1
ans.3
ans.4
(meta <- siena08(ans.1, ans.3, ans.4))
}
}
\keyword{models}
