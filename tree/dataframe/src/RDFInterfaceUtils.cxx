// Author: Enrico Guiraud, Danilo Piparo CERN  02/2018

/*************************************************************************
 * Copyright (C) 1995-2018, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#include <ROOT/RDataSource.hxx>
#include <ROOT/RTTreeDS.hxx>
#include <ROOT/RDF/InterfaceUtils.hxx>
#include <ROOT/RDF/RColumnRegister.hxx>
#include <ROOT/RDF/RDisplay.hxx>
#include <ROOT/RDF/RJittedDefine.hxx>
#include <ROOT/RDF/RJittedFilter.hxx>
#include <ROOT/RDF/RJittedVariation.hxx>
#include <ROOT/RDF/RLoopManager.hxx>
#include <ROOT/RDF/RNodeBase.hxx>
#include <ROOT/RDF/Utils.hxx>
#include <string_view>
#include <TBranch.h>
#include <TClass.h>
#include <TClassEdit.h>
#include <TDataType.h>
#include <TError.h>
#include <TLeaf.h>
#include <TObjArray.h>
#include <TPRegexp.h>
#include <TROOT.h>
#include <TString.h>
#include <TTree.h>
#include <TVirtualMutex.h>

// pragma to disable warnings on Rcpp which have
// so many noise compiling
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverloaded-virtual"
#pragma GCC diagnostic ignored "-Wshadow"
#endif
#include "lexertk.hpp"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <cassert>
#include <cstdlib>  // for size_t
#include <iterator> // for back_insert_iterator
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <unordered_set>
#include <utility> // for pair
#include <vector>

namespace ROOT::Detail::RDF {
class RDefineBase;
}

namespace {
using ROOT::Internal::RDF::IsStrInVec;
using ROOT::RDF::ColumnNames_t;

/// A string expression such as those passed to Filter and Define, digested to a standardized form
struct ParsedExpression {
   /// The string expression with the dummy variable names in fVarNames in place of the original column names
   std::string fExpr;
   /// The list of valid column names that were used in the original string expression.
   /// Duplicates are removed and column aliases (created with Alias calls) are resolved.
   ColumnNames_t fUsedCols;
   /// The list of variable names used in fExpr, with same ordering and size as fUsedCols
   ColumnNames_t fVarNames;
};

/// Look at expression `expr` and return a pair of (column names used, aliases used)
std::pair<ColumnNames_t, ColumnNames_t>
FindUsedColsAndAliases(const std::string &expr, const ColumnNames_t &treeBranchNames,
                       const ROOT::Internal::RDF::RColumnRegister &colRegister, const ColumnNames_t &dataSourceColNames)
{
   lexertk::generator tokens;
   const auto tokensOk = tokens.process(expr);
   if (!tokensOk) {
      const auto msg = "Failed to tokenize expression:\n" + expr + "\n\nMake sure it is valid C++.";
      throw std::runtime_error(msg);
   }

   std::unordered_set<std::string> usedCols;
   std::unordered_set<std::string> usedAliases;

   // iterate over tokens in expression and fill usedCols and usedAliases
   const auto nTokens = tokens.size();
   const auto kSymbol = lexertk::token::e_symbol;
   for (auto i = 0u; i < nTokens; ++i) {
      const auto &tok = tokens[i];
      // lexertk classifies '&' as e_symbol for some reason
      if (tok.type != kSymbol || tok.value == "&" || tok.value == "|") {
         // token is not a potential variable name, skip it
         continue;
      }

      ColumnNames_t potentialColNames({tok.value});

      // if token is the start of a dot chain (a.b.c...), a.b, a.b.c etc. are also potential column names
      auto dotChainKeepsGoing = [&](unsigned int _i) {
         return _i + 2 <= nTokens && tokens[_i + 1].value == "." && tokens[_i + 2].type == kSymbol;
      };
      while (dotChainKeepsGoing(i)) {
         potentialColNames.emplace_back(potentialColNames.back() + "." + tokens[i + 2].value);
         i += 2; // consume the tokens we looked at
      }

      // in an expression such as `a.b`, if `a` is a column alias add it to `usedAliases` and
      // replace the alias with the real column name in `potentialColNames`.
      const auto maybeAnAlias = potentialColNames[0]; // intentionally a copy as we'll modify potentialColNames later
      const auto &resolvedAlias = colRegister.ResolveAlias(maybeAnAlias);
      if (resolvedAlias != maybeAnAlias) { // this is an alias
         usedAliases.insert(maybeAnAlias);
         for (auto &s : potentialColNames)
            s.replace(0, maybeAnAlias.size(), resolvedAlias);
      }

      // find the longest potential column name that is an actual column name
      // (potential columns are sorted by length, so we search from the end to find the longest)
      auto isRDFColumn = [&](const std::string &col) {
         if (colRegister.IsDefineOrAlias(col) || IsStrInVec(col, treeBranchNames) ||
             IsStrInVec(col, dataSourceColNames))
            return true;
         return false;
      };
      const auto longestRDFColMatch = std::find_if(potentialColNames.crbegin(), potentialColNames.crend(), isRDFColumn);
      if (longestRDFColMatch != potentialColNames.crend())
         usedCols.insert(*longestRDFColMatch);
   }

   return {{usedCols.begin(), usedCols.end()}, {usedAliases.begin(), usedAliases.end()}};
}

/// Substitute each '.' in a string with '\.'
std::string EscapeDots(const std::string &s)
{
   TString out(s);
   TPRegexp dot("\\.");
   dot.Substitute(out, "\\.", "g");
   return std::string(std::move(out));
}

TString ResolveAliases(const TString &expr, const ColumnNames_t &usedAliases,
                              const ROOT::Internal::RDF::RColumnRegister &colRegister)
{
   TString out(expr);

   for (const auto &alias : usedAliases) {
      const auto &col = colRegister.ResolveAlias(alias);
      TPRegexp replacer("\\b" + EscapeDots(alias) + "\\b");
      replacer.Substitute(out, col.data(), "g");
   }

   return out;
}

ParsedExpression ParseRDFExpression(std::string_view expr, const ColumnNames_t &treeBranchNames,
                                           const ROOT::Internal::RDF::RColumnRegister &colRegister,
                                           const ColumnNames_t &dataSourceColNames)
{
   // transform `#var` into `R_rdf_sizeof_var`
   TString preProcessedExpr(expr);
   // match #varname at beginning of the sentence or after not-a-word, but exclude preprocessor directives like #ifdef
   TPRegexp colSizeReplacer(
      "(^|\\W)#(?!(ifdef|ifndef|if|else|elif|endif|pragma|define|undef|include|line))([a-zA-Z_][a-zA-Z0-9_]*)");
   colSizeReplacer.Substitute(preProcessedExpr, "$1R_rdf_sizeof_$3", "g");

   ColumnNames_t usedCols;
   ColumnNames_t usedAliases;
   std::tie(usedCols, usedAliases) =
      FindUsedColsAndAliases(std::string(preProcessedExpr), treeBranchNames, colRegister, dataSourceColNames);

   const auto exprNoAliases = ResolveAliases(preProcessedExpr, usedAliases, colRegister);

   // when we are done, exprWithVars willl be the same as preProcessedExpr but column names will be substituted with
   // the dummy variable names in varNames
   TString exprWithVars(exprNoAliases);

   ColumnNames_t varNames(usedCols.size());
   for (auto i = 0u; i < varNames.size(); ++i)
      varNames[i] = "var" + std::to_string(i);

   // sort the vector usedColsAndAliases by decreasing length of its elements,
   // so in case of friends we guarantee we never substitute a column name with another column containing it
   // ex. without sorting when passing "x" and "fr.x", the replacer would output "var0" and "fr.var0",
   // because it has already substituted "x", hence the "x" in "fr.x" would be recognized as "var0",
   // whereas the desired behaviour is handling them as "var0" and "var1"
   std::sort(usedCols.begin(), usedCols.end(),
             [](const std::string &a, const std::string &b) { return a.size() > b.size(); });
   for (const auto &col : usedCols) {
      const auto varIdx = std::distance(usedCols.begin(), std::find(usedCols.begin(), usedCols.end(), col));
      TPRegexp replacer("\\b" + EscapeDots(col) + "\\b");
      replacer.Substitute(exprWithVars, varNames[varIdx], "g");
   }

   return ParsedExpression{std::string(std::move(exprWithVars)), std::move(usedCols), std::move(varNames)};
}

/// Return the static global map of Filter/Define functions that have been jitted.
/// It's used to check whether a given expression has already been jitted, and
/// to look up its associated variable name if it is.
/// Keys in the map are the body of the expression, values are the name of the
/// jitted variable that corresponds to that expression. For example, for:
///     auto f1(){ return 42; }
/// key would be "(){ return 42; }" and value would be "f1".
std::unordered_map<std::string, std::string> &GetJittedExprs() {
   static std::unordered_map<std::string, std::string> jittedExpressions;
   return jittedExpressions;
}

std::string
BuildFunctionString(const std::string &expr, const ColumnNames_t &vars, const ColumnNames_t &varTypes)
{
   assert(vars.size() == varTypes.size());

   TPRegexp re(R"(\breturn\b)");
   const bool hasReturnStmt = re.MatchB(expr);

   static const std::vector<std::string> fundamentalTypes = {
      "int",
      "signed",
      "signed int",
      "Int_t",
      "unsigned",
      "unsigned int",
      "UInt_t",
      "double",
      "Double_t",
      "float",
      "Float_t",
      "char",
      "Char_t",
      "unsigned char",
      "UChar_t",
      "bool",
      "Bool_t",
      "short",
      "short int",
      "Short_t",
      "long",
      "long int",
      "long long int",
      "Long64_t",
      "unsigned long",
      "unsigned long int",
      "ULong64_t",
      "std::size_t",
      "size_t",
      "Ssiz_t"
   };

   std::stringstream ss;
   ss << "(";
   for (auto i = 0u; i < vars.size(); ++i) {
      std::string fullType;
      const auto &type = varTypes[i];
      if (std::find(fundamentalTypes.begin(), fundamentalTypes.end(), type) != fundamentalTypes.end()) {
         // pass it by const value to help detect common mistakes such as if(x = 3)
         fullType = "const " + type + " ";
      } else {
         // We pass by reference to avoid expensive copies
         // It can't be const reference in general, as users might want/need to call non-const methods on the values
         fullType = type + "& ";
      }
      ss << fullType << vars[i] << ", ";
   }
   if (!vars.empty())
      ss.seekp(-2, ss.cur);

   if (hasReturnStmt)
      ss << "){";
   else
      ss << "){return ";
   ss << expr << "\n;}";

   return ss.str();
}

/// Declare a function to the interpreter in namespace R_rdf, return the name of the jitted function.
/// If the function is already in GetJittedExprs, return the name for the function that has already been jitted.
std::string DeclareFunction(const std::string &expr, const ColumnNames_t &vars, const ColumnNames_t &varTypes)
{
   R__LOCKGUARD(gROOTMutex);

   const auto funcCode = BuildFunctionString(expr, vars, varTypes);
   auto &exprMap = GetJittedExprs();
   const auto exprIt = exprMap.find(funcCode);
   if (exprIt != exprMap.end()) {
      // expression already there
      const auto funcName = exprIt->second;
      return funcName;
   }

   // new expression
   const auto funcBaseName = "func" + std::to_string(exprMap.size());
   const auto funcFullName = "R_rdf::" + funcBaseName;

   const auto toDeclare = "namespace R_rdf {\nauto " + funcBaseName + funcCode + "\nusing " + funcBaseName +
                          "_ret_t = typename ROOT::TypeTraits::CallableTraits<decltype(" + funcBaseName +
                          ")>::ret_type;\n}";
   ROOT::Internal::RDF::InterpreterDeclare(toDeclare);

   // InterpreterDeclare could throw. If it doesn't, mark the function as already jitted
   exprMap.insert({funcCode, funcFullName});

   return funcFullName;
}

/// Each jitted function comes with a func_ret_t type alias for its return type.
/// Resolve that alias and return the true type as string.
std::string RetTypeOfFunc(const std::string &funcName)
{
   const auto dt = gROOT->GetType((funcName + "_ret_t").c_str());
   R__ASSERT(dt != nullptr);
   const auto type = dt->GetFullTypeName();
   return type;
}

[[noreturn]] void
ThrowJitBuildActionHelperTypeError(const std::string &actionTypeNameBase, const std::type_info &helperArgType)
{
   int err = 0;
   const char *cname = TClassEdit::DemangleTypeIdName(helperArgType, err);
   std::string actionHelperTypeName = cname;
   delete[] cname;
   if (err != 0)
      actionHelperTypeName = helperArgType.name();

   std::string exceptionText =
      "RDataFrame::Jit: cannot just-in-time compile a \"" + actionTypeNameBase + "\" action using helper type \"" +
      actionHelperTypeName +
      "\". This typically happens in a custom `Fill` or `Book` invocation where the types of the input columns have "
      "not been specified as template parameters and the ROOT interpreter has no knowledge of this type of action "
      "helper. Please add template parameters for the types of the input columns to avoid jitting this action (i.e. "
      "`df.Fill<float>(..., {\"x\"})`, where `float` is the type of `x`) or declare the action helper type to the "
      "interpreter, e.g. via gInterpreter->Declare.";

   throw std::runtime_error(exceptionText);
}

} // anonymous namespace

namespace ROOT {
namespace Internal {
namespace RDF {

/// Take a list of column names, return that list with entries starting by '#' filtered out.
/// The function throws when filtering out a column this way.
ColumnNames_t FilterArraySizeColNames(const ColumnNames_t &columnNames, const std::string &action)
{
   ColumnNames_t columnListWithoutSizeColumns;
   ColumnNames_t filteredColumns;
   std::copy_if(columnNames.begin(), columnNames.end(), std::back_inserter(columnListWithoutSizeColumns),
                [&](const std::string &name) {
                   if (name[0] == '#') {
                     filteredColumns.emplace_back(name);
                      return false;
                   } else {
                      return true;
                   }
                });

   if (!filteredColumns.empty()) {
      std::string msg = "Column name(s) {";
      for (auto &c : filteredColumns)
         msg += c + ", ";
      msg[msg.size() - 2] = '}';
      msg += "will be ignored. Please go through a valid Alias to " + action + " an array size column";
      throw std::runtime_error(msg);
   }

   return columnListWithoutSizeColumns;
}

std::string ResolveAlias(const std::string &col, const std::map<std::string, std::string> &aliasMap)
{
   const auto it = aliasMap.find(col);
   if (it != aliasMap.end())
      return it->second;

   // #var is an alias for R_rdf_sizeof_var
   if (col.size() > 1 && col[0] == '#')
      return "R_rdf_sizeof_" + col.substr(1);

   return col;
}

void CheckValidCppVarName(std::string_view var, const std::string &where)
{
   bool isValid = true;

   if (var.empty())
      isValid = false;
   const char firstChar = var[0];

   // first character must be either a letter or an underscore
   auto isALetter = [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); };
   const bool isValidFirstChar = firstChar == '_' || isALetter(firstChar);
   if (!isValidFirstChar)
      isValid = false;

   // all characters must be either a letter, an underscore or a number
   auto isANumber = [](char c) { return c >= '0' && c <= '9'; };
   auto isValidTok = [&isALetter, &isANumber](char c) { return c == '_' || isALetter(c) || isANumber(c); };
   for (const char c : var)
      if (!isValidTok(c))
         isValid = false;

   if (!isValid) {
      const auto objName = where == "Define" ? "column" : "variation";
      const auto error = "RDataFrame::" + where + ": cannot define " + objName + " \"" + std::string(var) +
                         "\". Not a valid C++ variable name.";
      throw std::runtime_error(error);
   }
}

std::string DemangleTypeIdName(const std::type_info &typeInfo)
{
   int dummy(0);
   char *tn = TClassEdit::DemangleTypeIdName(typeInfo, dummy);
   std::string tname(tn);
   free(tn);
   return tname;
}

ColumnNames_t
ConvertRegexToColumns(const ColumnNames_t &colNames, std::string_view columnNameRegexp, std::string_view callerName)
{
   const auto theRegexSize = columnNameRegexp.size();
   std::string theRegex(columnNameRegexp);

   const auto isEmptyRegex = 0 == theRegexSize;
   // This is to avoid cases where branches called b1, b2, b3 are all matched by expression "b"
   if (theRegexSize > 0 && theRegex[0] != '^')
      theRegex = "^" + theRegex;
   if (theRegexSize > 0 && theRegex[theRegexSize - 1] != '$')
      theRegex = theRegex + "$";

   ColumnNames_t selectedColumns;

   // Since we support gcc48 and it does not provide in its stl std::regex,
   // we need to use TPRegexp
   TPRegexp regexp(theRegex);
   for (auto &&colName : colNames) {
      if ((isEmptyRegex || regexp.MatchB(colName.c_str())) && !IsInternalColumn(colName)) {
         selectedColumns.emplace_back(colName);
      }
   }

   if (selectedColumns.empty()) {
      std::string text(callerName);
      if (columnNameRegexp.empty()) {
         text = ": there is no column available to match.";
      } else {
         text = ": regex \"" + std::string(columnNameRegexp) + "\" did not match any column.";
      }
      throw std::runtime_error(text);
   }
   return selectedColumns;
}

/// Throw if column `definedColView` is already there.
void CheckForRedefinition(const std::string &where, std::string_view definedColView, const RColumnRegister &colRegister,
                          const ColumnNames_t &treeColumns, const ColumnNames_t &dataSourceColumns)
{

   std::string error{};
   if (colRegister.IsAlias(definedColView))
      error = "An alias with that name, pointing to column \"" + std::string(colRegister.ResolveAlias(definedColView)) +
              "\", already exists in this branch of the computation graph.";
   else if (colRegister.IsDefineOrAlias(definedColView))
      error = "A column with that name has already been Define'd. Use Redefine to force redefinition.";
   // else, check if definedColView is in the list of tree branches. This is a bit better than interrogating the TTree
   // directly because correct usage of GetBranch, FindBranch, GetLeaf and FindLeaf can be tricky; so let's assume we
   // got it right when we collected the list of available branches.
   else if (std::find(treeColumns.begin(), treeColumns.end(), definedColView) != treeColumns.end())
      error =
         "A branch with that name is already present in the input TTree/TChain. Use Redefine to force redefinition.";
   else if (std::find(dataSourceColumns.begin(), dataSourceColumns.end(), definedColView) != dataSourceColumns.end())
      error =
         "A column with that name is already present in the input data source. Use Redefine to force redefinition.";

   if (!error.empty()) {
      error = "RDataFrame::" + where + ": cannot define column \"" + std::string(definedColView) + "\". " + error;
      throw std::runtime_error(error);
   }
}

/// Throw if column `definedColView` is _not_ already there.
void CheckForDefinition(const std::string &where, std::string_view definedColView, const RColumnRegister &colRegister,
                        const ColumnNames_t &treeColumns, const ColumnNames_t &dataSourceColumns)
{
   std::string error{};

   if (colRegister.IsAlias(definedColView)) {
      error = "An alias with that name, pointing to column \"" + std::string(colRegister.ResolveAlias(definedColView)) +
              "\", already exists. Aliases cannot be Redefined or Varied.";
   }

   if (error.empty()) {
      const bool isAlreadyDefined = colRegister.IsDefineOrAlias(definedColView);
      // check if definedCol is in the list of tree branches. This is a bit better than interrogating the TTree
      // directly because correct usage of GetBranch, FindBranch, GetLeaf and FindLeaf can be tricky; so let's assume we
      // got it right when we collected the list of available branches.
      const bool isABranch = std::find(treeColumns.begin(), treeColumns.end(), definedColView) != treeColumns.end();
      const bool isADSColumn =
         std::find(dataSourceColumns.begin(), dataSourceColumns.end(), definedColView) != dataSourceColumns.end();

      if (!isAlreadyDefined && !isABranch && !isADSColumn)
         error = "No column with that name was found in the dataset. Use Define to create a new column.";
   }

   if (!error.empty()) {
      if (where == "DefaultValueFor")
         error = "RDataFrame::" + where + ": cannot provide default values for column \"" +
                 std::string(definedColView) + "\". " + error;
      else
         error = "RDataFrame::" + where + ": cannot redefine or vary column \"" + std::string(definedColView) + "\". " +
                 error;
      throw std::runtime_error(error);
   }
}

/// Throw if the column has systematic variations attached.
void CheckForNoVariations(const std::string &where, std::string_view definedColView, const RColumnRegister &colRegister)
{
   const std::string definedCol(definedColView);
   const auto &variationDeps = colRegister.GetVariationDeps(definedCol);
   if (!variationDeps.empty()) {
      if (where == "Redefine") {
         const std::string error = "RDataFrame::" + where + ": cannot redefine column \"" + definedCol +
                                   "\". The column depends on one or more systematic variations and re-defining varied "
                                   "columns is not supported.";
         throw std::runtime_error(error);
      } else if (where == "DefaultValueFor") {
         const std::string error = "RDataFrame::" + where + ": cannot provide a default value for column \"" +
                                   definedCol +
                                   "\". The column depends on one or more systematic variations and it should not be "
                                   "possible to have missing values in varied columns.";
         throw std::runtime_error(error);
      } else {
         const std::string error =
            "RDataFrame::" + where + ": this operation cannot work with columns that depend on systematic variations.";
         throw std::runtime_error(error);
      }
   }
}

void CheckTypesAndPars(unsigned int nTemplateParams, unsigned int nColumnNames)
{
   if (nTemplateParams != nColumnNames) {
      std::string err_msg = "The number of template parameters specified is ";
      err_msg += std::to_string(nTemplateParams);
      err_msg += " while ";
      err_msg += std::to_string(nColumnNames);
      err_msg += " columns have been specified.";
      throw std::runtime_error(err_msg);
   }
}

/// Choose between local column names or default column names, throw in case of errors.
const ColumnNames_t
SelectColumns(unsigned int nRequiredNames, const ColumnNames_t &names, const ColumnNames_t &defaultNames)
{
   if (names.empty()) {
      // use default column names
      if (defaultNames.size() < nRequiredNames)
         throw std::runtime_error(
            std::to_string(nRequiredNames) + " column name" + (nRequiredNames == 1 ? " is" : "s are") +
            " required but none were provided and the default list has size " + std::to_string(defaultNames.size()));
      // return first nRequiredNames default column names
      return ColumnNames_t(defaultNames.begin(), defaultNames.begin() + nRequiredNames);
   } else {
      // use column names provided by the user to this particular transformation/action
      if (names.size() != nRequiredNames) {
         auto msg = std::to_string(nRequiredNames) + " column name" + (nRequiredNames == 1 ? " is" : "s are") +
                    " required but " + std::to_string(names.size()) + (names.size() == 1 ? " was" : " were") +
                    " provided:";
         for (const auto &name : names)
            msg += " \"" + name + "\",";
         msg.back() = '.';
         throw std::runtime_error(msg);
      }
      return names;
   }
}

ColumnNames_t FindUnknownColumns(const ColumnNames_t &requiredCols, const ColumnNames_t &datasetColumns,
                                 const RColumnRegister &definedCols, const ColumnNames_t &dataSourceColumns)
{
   ColumnNames_t unknownColumns;
   for (auto &column : requiredCols) {
      const auto isBranch = std::find(datasetColumns.begin(), datasetColumns.end(), column) != datasetColumns.end();
      if (isBranch)
         continue;
      if (definedCols.IsDefineOrAlias(column))
         continue;
      const auto isDataSourceColumn =
         std::find(dataSourceColumns.begin(), dataSourceColumns.end(), column) != dataSourceColumns.end();
      if (isDataSourceColumn)
         continue;
      unknownColumns.emplace_back(column);
   }
   return unknownColumns;
}

std::vector<std::string> GetFilterNames(const std::shared_ptr<RLoopManager> &loopManager)
{
   return loopManager->GetFiltersNames();
}

ParsedTreePath ParseTreePath(std::string_view fullTreeName)
{
   // split name into directory and treename if needed
   std::string_view dirName = "";
   std::string_view treeName = fullTreeName;
   const auto lastSlash = fullTreeName.rfind('/');
   if (std::string_view::npos != lastSlash) {
      dirName = treeName.substr(0, lastSlash);
      treeName = treeName.substr(lastSlash + 1, treeName.size());
   }
   return {std::string(treeName), std::string(dirName)};
}

std::string PrettyPrintAddr(const void *const addr)
{
   std::stringstream s;
   // Windows-friendly
   s << std::hex << std::showbase << reinterpret_cast<size_t>(addr);
   return s.str();
}

/// Book the jitting of a Filter call
std::shared_ptr<RDFDetail::RJittedFilter>
BookFilterJit(std::shared_ptr<RDFDetail::RNodeBase> *prevNodeOnHeap, std::string_view name, std::string_view expression,
              const ColumnNames_t &branches, const RColumnRegister &colRegister, TTree *tree, RDataSource *ds)
{
   const auto &dsColumns = ds ? ds->GetColumnNames() : ColumnNames_t{};

   const auto parsedExpr = ParseRDFExpression(expression, branches, colRegister, dsColumns);
   const auto exprVarTypes =
      GetValidatedArgTypes(parsedExpr.fUsedCols, colRegister, tree, ds, "Filter", /*vector2RVec=*/true);
   const auto funcName = DeclareFunction(parsedExpr.fExpr, parsedExpr.fVarNames, exprVarTypes);
   const auto type = RetTypeOfFunc(funcName);
   if (type != "bool")
      std::runtime_error("Filter: the following expression does not evaluate to bool:\n" + std::string(expression));

   // definesOnHeap is deleted by the jitted call to JitFilterHelper
   ROOT::Internal::RDF::RColumnRegister *definesOnHeap = new ROOT::Internal::RDF::RColumnRegister(colRegister);
   const auto definesOnHeapAddr = PrettyPrintAddr(definesOnHeap);
   const auto prevNodeAddr = PrettyPrintAddr(prevNodeOnHeap);

   const auto jittedFilter = std::make_shared<RDFDetail::RJittedFilter>(
      (*prevNodeOnHeap)->GetLoopManagerUnchecked(), name,
      Union(colRegister.GetVariationDeps(parsedExpr.fUsedCols), (*prevNodeOnHeap)->GetVariations()));

   // Produce code snippet that creates the filter and registers it with the corresponding RJittedFilter
   // Windows requires std::hex << std::showbase << (size_t)pointer to produce notation "0x1234"
   std::stringstream filterInvocation;
   filterInvocation << "ROOT::Internal::RDF::JitFilterHelper(" << funcName << ", new const char*["
                    << parsedExpr.fUsedCols.size() << "]{";
   for (const auto &col : parsedExpr.fUsedCols)
      filterInvocation << "\"" << col << "\", ";
   if (!parsedExpr.fUsedCols.empty())
      filterInvocation.seekp(-2, filterInvocation.cur); // remove the last ",
   // lifetime of pointees:
   // - jittedFilter: heap-allocated weak_ptr to the actual jittedFilter that will be deleted by JitFilterHelper
   // - prevNodeOnHeap: heap-allocated shared_ptr to the actual previous node that will be deleted by JitFilterHelper
   // - definesOnHeap: heap-allocated, will be deleted by JitFilterHelper
   filterInvocation << "}, " << parsedExpr.fUsedCols.size() << ", \"" << name << "\", "
                    << "reinterpret_cast<std::weak_ptr<ROOT::Detail::RDF::RJittedFilter>*>("
                    << PrettyPrintAddr(MakeWeakOnHeap(jittedFilter)) << "), "
                    << "reinterpret_cast<std::shared_ptr<ROOT::Detail::RDF::RNodeBase>*>(" << prevNodeAddr << "),"
                    << "reinterpret_cast<ROOT::Internal::RDF::RColumnRegister*>(" << definesOnHeapAddr << ")"
                    << ");\n";

   auto lm = jittedFilter->GetLoopManagerUnchecked();
   lm->ToJitExec(filterInvocation.str());

   return jittedFilter;
}

/// Book the jitting of a Define call
std::shared_ptr<RJittedDefine> BookDefineJit(std::string_view name, std::string_view expression, RLoopManager &lm,
                                             RDataSource *ds, const RColumnRegister &colRegister,
                                             const ColumnNames_t &branches,
                                             std::shared_ptr<RNodeBase> *upcastNodeOnHeap)
{
   auto *const tree = lm.GetTree();
   const auto &dsColumns = ds ? ds->GetColumnNames() : ColumnNames_t{};

   const auto parsedExpr = ParseRDFExpression(expression, branches, colRegister, dsColumns);
   const auto exprVarTypes =
      GetValidatedArgTypes(parsedExpr.fUsedCols, colRegister, tree, ds, "Define", /*vector2RVec=*/true);
   const auto funcName = DeclareFunction(parsedExpr.fExpr, parsedExpr.fVarNames, exprVarTypes);
   const auto type = RetTypeOfFunc(funcName);

   auto definesCopy = new RColumnRegister(colRegister);
   auto definesAddr = PrettyPrintAddr(definesCopy);
   auto jittedDefine = std::make_shared<RDFDetail::RJittedDefine>(name, type, lm, colRegister, parsedExpr.fUsedCols);

   std::stringstream defineInvocation;
   defineInvocation << "ROOT::Internal::RDF::JitDefineHelper<ROOT::Internal::RDF::DefineTypes::RDefineTag>(" << funcName
                    << ", new const char*[" << parsedExpr.fUsedCols.size() << "]{";
   for (const auto &col : parsedExpr.fUsedCols) {
      defineInvocation << "\"" << col << "\", ";
   }
   if (!parsedExpr.fUsedCols.empty())
      defineInvocation.seekp(-2, defineInvocation.cur); // remove the last ",
   // lifetime of pointees:
   // - lm is the loop manager, and if that goes out of scope jitting does not happen at all (i.e. will always be valid)
   // - jittedDefine: heap-allocated weak_ptr that will be deleted by JitDefineHelper after usage
   // - definesAddr: heap-allocated, will be deleted by JitDefineHelper after usage
   defineInvocation << "}, " << parsedExpr.fUsedCols.size() << ", \"" << name
                    << "\", reinterpret_cast<ROOT::Detail::RDF::RLoopManager*>(" << PrettyPrintAddr(&lm)
                    << "), reinterpret_cast<std::weak_ptr<ROOT::Detail::RDF::RJittedDefine>*>("
                    << PrettyPrintAddr(MakeWeakOnHeap(jittedDefine))
                    << "), reinterpret_cast<ROOT::Internal::RDF::RColumnRegister*>(" << definesAddr
                    << "), reinterpret_cast<std::shared_ptr<ROOT::Detail::RDF::RNodeBase>*>("
                    << PrettyPrintAddr(upcastNodeOnHeap) << "));\n";

   lm.ToJitExec(defineInvocation.str());
   return jittedDefine;
}

/// Book the jitting of a DefinePerSample call
std::shared_ptr<RJittedDefine> BookDefinePerSampleJit(std::string_view name, std::string_view expression,
                                                      RLoopManager &lm, const RColumnRegister &colRegister,
                                                      std::shared_ptr<RNodeBase> *upcastNodeOnHeap)
{
   const auto funcName = DeclareFunction(std::string(expression), {"rdfslot_", "rdfsampleinfo_"},
                                         {"unsigned int", "const ROOT::RDF::RSampleInfo"});
   const auto retType = RetTypeOfFunc(funcName);

   auto definesCopy = new RColumnRegister(colRegister);
   auto definesAddr = PrettyPrintAddr(definesCopy);
   auto jittedDefine = std::make_shared<RDFDetail::RJittedDefine>(name, retType, lm, colRegister, ColumnNames_t{});

   std::stringstream defineInvocation;
   defineInvocation << "ROOT::Internal::RDF::JitDefineHelper<ROOT::Internal::RDF::DefineTypes::RDefinePerSampleTag>("
                    << funcName << ", nullptr, 0, ";
   // lifetime of pointees:
   // - lm is the loop manager, and if that goes out of scope jitting does not happen at all (i.e. will always be valid)
   // - jittedDefine: heap-allocated weak_ptr that will be deleted by JitDefineHelper after usage
   // - definesAddr: heap-allocated, will be deleted by JitDefineHelper after usage
   defineInvocation << "\"" << name << "\", reinterpret_cast<ROOT::Detail::RDF::RLoopManager*>(" << PrettyPrintAddr(&lm)
                    << "), reinterpret_cast<std::weak_ptr<ROOT::Detail::RDF::RJittedDefine>*>("
                    << PrettyPrintAddr(MakeWeakOnHeap(jittedDefine))
                    << "), reinterpret_cast<ROOT::Internal::RDF::RColumnRegister*>(" << definesAddr
                    << "), reinterpret_cast<std::shared_ptr<ROOT::Detail::RDF::RNodeBase>*>("
                    << PrettyPrintAddr(upcastNodeOnHeap) << "));\n";

   lm.ToJitExec(defineInvocation.str());
   return jittedDefine;
}

/// Book the jitting of a Vary call
std::shared_ptr<RJittedVariation>
BookVariationJit(const std::vector<std::string> &colNames, std::string_view variationName,
                 const std::vector<std::string> &variationTags, std::string_view expression, RLoopManager &lm,
                 RDataSource *ds, const RColumnRegister &colRegister, const ColumnNames_t &branches,
                 std::shared_ptr<RNodeBase> *upcastNodeOnHeap, bool isSingleColumn)
{
   auto *const tree = lm.GetTree();
   const auto &dsColumns = ds ? ds->GetColumnNames() : ColumnNames_t{};

   const auto parsedExpr = ParseRDFExpression(expression, branches, colRegister, dsColumns);
   const auto exprVarTypes =
      GetValidatedArgTypes(parsedExpr.fUsedCols, colRegister, tree, ds, "Vary", /*vector2RVec=*/true);
   const auto funcName = DeclareFunction(parsedExpr.fExpr, parsedExpr.fVarNames, exprVarTypes);
   const auto type = RetTypeOfFunc(funcName);

   if (type.rfind("ROOT::VecOps::RVec", 0) != 0) {
      // Avoid leak
      delete upcastNodeOnHeap;
      upcastNodeOnHeap = nullptr;
      throw std::runtime_error(
         "Jitted Vary expressions must return an RVec object. The following expression returns a " + type +
         " instead:\n" + parsedExpr.fExpr);
   }

   auto colRegisterCopy = new RColumnRegister(colRegister);
   const auto colRegisterAddr = PrettyPrintAddr(colRegisterCopy);
   auto jittedVariation = std::make_shared<RJittedVariation>(colNames, variationName, variationTags, type, colRegister,
                                                             lm, parsedExpr.fUsedCols);

   // build invocation to JitVariationHelper
   // arrays of strings are passed as const char** plus size.
   // lifetime of pointees:
   // - lm is the loop manager, and if that goes out of scope jitting does not happen at all (i.e. will always be valid)
   // - jittedVariation: heap-allocated weak_ptr that will be deleted by JitDefineHelper after usage
   // - definesAddr: heap-allocated, will be deleted by JitDefineHelper after usage
   std::stringstream varyInvocation;
   varyInvocation << "ROOT::Internal::RDF::JitVariationHelper<" << (isSingleColumn ? "true" : "false") << ">("
                  << funcName << ", new const char*[" << parsedExpr.fUsedCols.size() << "]{";
   for (const auto &col : parsedExpr.fUsedCols) {
      varyInvocation << "\"" << col << "\", ";
   }
   if (!parsedExpr.fUsedCols.empty())
      varyInvocation.seekp(-2, varyInvocation.cur); // remove the last ", "
   varyInvocation << "}, " << parsedExpr.fUsedCols.size();
   varyInvocation << ", new const char*[" << colNames.size() << "]{";
   for (const auto &col : colNames) {
      varyInvocation << "\"" << col << "\", ";
   }
   varyInvocation.seekp(-2, varyInvocation.cur); // remove the last ", "
   varyInvocation << "}, " << colNames.size() << ", new const char*[" << variationTags.size() << "]{";
   for (const auto &tag : variationTags) {
      varyInvocation << "\"" << tag << "\", ";
   }
   varyInvocation.seekp(-2, varyInvocation.cur); // remove the last ", "
   varyInvocation << "}, " << variationTags.size() << ", \"" << variationName
                  << "\", reinterpret_cast<ROOT::Detail::RDF::RLoopManager*>(" << PrettyPrintAddr(&lm)
                  << "), reinterpret_cast<std::weak_ptr<ROOT::Internal::RDF::RJittedVariation>*>("
                  << PrettyPrintAddr(MakeWeakOnHeap(jittedVariation))
                  << "), reinterpret_cast<ROOT::Internal::RDF::RColumnRegister*>(" << colRegisterAddr
                  << "), reinterpret_cast<std::shared_ptr<ROOT::Detail::RDF::RNodeBase>*>("
                  << PrettyPrintAddr(upcastNodeOnHeap) << "));\n";

   lm.ToJitExec(varyInvocation.str());
   return jittedVariation;
}

// Jit and call something equivalent to "this->BuildAndBook<ColTypes...>(params...)"
// (see comments in the body for actual jitted code)
std::string JitBuildAction(const ColumnNames_t &cols, std::shared_ptr<RDFDetail::RNodeBase> *prevNode,
                           const std::type_info &helperArgType, const std::type_info &at, void *helperArgOnHeap,
                           TTree *tree, const unsigned int nSlots, const RColumnRegister &colRegister, RDataSource *ds,
                           std::weak_ptr<RJittedAction> *jittedActionOnHeap, const bool vector2RVec)
{
   // retrieve type of action as a string
   auto actionTypeClass = TClass::GetClass(at);
   if (!actionTypeClass) {
      std::string exceptionText = "An error occurred while inferring the action type of the operation.";
      throw std::runtime_error(exceptionText);
   }
   const std::string actionTypeName = actionTypeClass->GetName();
   const std::string actionTypeNameBase = actionTypeName.substr(actionTypeName.rfind(':') + 1);

   // retrieve type of result of the action as a string
   const auto helperArgTypeName = TypeID2TypeName(helperArgType);
   if (helperArgTypeName.empty()) {
      ThrowJitBuildActionHelperTypeError(actionTypeNameBase, helperArgType);
   }

   auto definesCopy = new RColumnRegister(colRegister); // deleted in jitted CallBuildAction
   auto definesAddr = PrettyPrintAddr(definesCopy);

   // Build a call to CallBuildAction with the appropriate argument. When run through the interpreter, this code will
   // just-in-time create an RAction object and it will assign it to its corresponding RJittedAction.
   std::stringstream createAction_str;
   createAction_str << "ROOT::Internal::RDF::CallBuildAction<" << actionTypeName;
   const auto columnTypeNames = GetValidatedArgTypes(cols, colRegister, tree, ds, actionTypeNameBase, vector2RVec);
   for (auto &colType : columnTypeNames)
      createAction_str << ", " << colType;
   // on Windows, to prefix the hexadecimal value of a pointer with '0x',
   // one need to write: std::hex << std::showbase << (size_t)pointer
   createAction_str << ">(reinterpret_cast<std::shared_ptr<ROOT::Detail::RDF::RNodeBase>*>("
                    << PrettyPrintAddr(prevNode) << "), new const char*[" << cols.size() << "]{";
   for (auto i = 0u; i < cols.size(); ++i) {
      if (i != 0u)
         createAction_str << ", ";
      createAction_str << '"' << cols[i] << '"';
   }
   createAction_str << "}, " << cols.size() << ", " << nSlots << ", reinterpret_cast<shared_ptr<" << helperArgTypeName
                    << ">*>(" << PrettyPrintAddr(helperArgOnHeap)
                    << "), reinterpret_cast<std::weak_ptr<ROOT::Internal::RDF::RJittedAction>*>("
                    << PrettyPrintAddr(jittedActionOnHeap)
                    << "), reinterpret_cast<ROOT::Internal::RDF::RColumnRegister*>(" << definesAddr << "));";
   return createAction_str.str();
}

bool AtLeastOneEmptyString(const std::vector<std::string_view> strings)
{
   for (const auto &s : strings) {
      if (s.empty())
         return true;
   }
   return false;
}

std::shared_ptr<RNodeBase> UpcastNode(std::shared_ptr<RNodeBase> ptr)
{
   return ptr;
}

/// Given the desired number of columns and the user-provided list of columns:
/// * fallback to using the first nColumns default columns if needed (or throw if nColumns > nDefaultColumns)
/// * check that selected column names refer to valid branches, custom columns or datasource columns (throw if not)
/// * replace column names from aliases by the actual column name
/// Return the list of selected column names.
ColumnNames_t GetValidatedColumnNames(RLoopManager &lm, const unsigned int nColumns, const ColumnNames_t &columns,
                                      const RColumnRegister &colRegister, RDataSource *ds)
{
   auto selectedColumns = SelectColumns(nColumns, columns, lm.GetDefaultColumnNames());

   for (auto &col : selectedColumns) {
      col = colRegister.ResolveAlias(col);
   }

   // Complain if there are still unknown columns at this point
   auto unknownColumns = FindUnknownColumns(selectedColumns, lm.GetBranchNames(), colRegister,
                                            ds ? ds->GetColumnNames() : ColumnNames_t{});

   if (!unknownColumns.empty()) {
      // Some columns are still unknown, we need to understand if the error
      // should be printed or if the user requested to explicitly disable it.
      // Look for a possible overlap between the unknown columns and the
      // columns we should ignore for the purpose of the following exception
      std::set<std::string> intersection;
      const auto &colsToIgnore = lm.GetSuppressErrorsForMissingBranches();
      std::sort(unknownColumns.begin(), unknownColumns.end());
      std::set_intersection(unknownColumns.cbegin(), unknownColumns.cend(), colsToIgnore.cbegin(), colsToIgnore.cend(),
                            std::inserter(intersection, intersection.begin()));
      if (intersection.empty()) {
         std::string errMsg = std::string("Unknown column") + (unknownColumns.size() > 1 ? "s: " : ": ");
         for (auto &unknownColumn : unknownColumns)
            errMsg += '"' + unknownColumn + "\", ";
         errMsg.resize(errMsg.size() - 2); // remove last ", "
         throw std::runtime_error(errMsg);
      }
   }

   return selectedColumns;
}

std::vector<std::string> GetValidatedArgTypes(const ColumnNames_t &colNames, const RColumnRegister &colRegister,
                                              TTree *tree, RDataSource *ds, const std::string &context,
                                              bool vector2RVec)
{
   auto toCheckedArgType = [&](const std::string &c) {
      RDFDetail::RDefineBase *define = colRegister.GetDefine(c);
      const auto colType = ColumnName2ColumnTypeName(c, tree, ds, define, vector2RVec);
      if (colType.rfind("CLING_UNKNOWN_TYPE", 0) == 0) { // the interpreter does not know this type
         const auto msg =
            "The type of custom column \"" + c + "\" (" + colType.substr(19) +
            ") is not known to the interpreter, but a just-in-time-compiled " + context +
            " call requires this column. Make sure to create and load ROOT dictionaries for this column's class.";
         throw std::runtime_error(msg);
      }
      return colType;
   };
   std::vector<std::string> colTypes;
   colTypes.reserve(colNames.size());
   std::transform(colNames.begin(), colNames.end(), std::back_inserter(colTypes), toCheckedArgType);
   return colTypes;
}

/// Return a bitset each element of which indicates whether the corresponding element in `selectedColumns` is the
/// name of a column that must be defined via datasource. All elements of the returned vector are false if no
/// data-source is present.
std::vector<bool> FindUndefinedDSColumns(const ColumnNames_t &requestedCols, const ColumnNames_t &definedCols)
{
   const auto nColumns = requestedCols.size();
   std::vector<bool> mustBeDefined(nColumns, false);
   for (auto i = 0u; i < nColumns; ++i)
      mustBeDefined[i] = std::find(definedCols.begin(), definedCols.end(), requestedCols[i]) == definedCols.end();
   return mustBeDefined;
}

void CheckForDuplicateSnapshotColumns(const ColumnNames_t &cols)
{
   std::unordered_set<std::string> uniqueCols;
   for (auto &col : cols) {
      if (!uniqueCols.insert(col).second) {
         const auto msg = "Error: column \"" + col +
                          "\" was passed to Snapshot twice. This is not supported: only one of the columns would be "
                          "readable with RDataFrame.";
         throw std::logic_error(msg);
      }
   }
}

/// Return copies of colsWithoutAliases and colsWithAliases with size branches for variable-sized array branches added
/// in the right positions (i.e. before the array branches that need them).
std::pair<std::vector<std::string>, std::vector<std::string>>
AddSizeBranches(const std::vector<std::string> &branches, ROOT::RDF::RDataSource *ds,
                std::vector<std::string> &&colsWithoutAliases, std::vector<std::string> &&colsWithAliases)
{
   TTree *tree{};
   if (auto treeDS = dynamic_cast<ROOT::Internal::RDF::RTTreeDS *>(ds))
      tree = treeDS->GetTree();
   if (!tree) // nothing to do
      return {std::move(colsWithoutAliases), std::move(colsWithAliases)};

   assert(colsWithoutAliases.size() == colsWithAliases.size());

   auto nCols = colsWithoutAliases.size();
   // Use index-iteration as we modify the vector during the iteration.
   for (std::size_t i = 0u; i < nCols; ++i) {
      const auto &colName = colsWithoutAliases[i];
      if (!IsStrInVec(colName, branches))
         continue; // this column is not a TTree branch, nothing to do

      auto *b = tree->GetBranch(colName.c_str());
      if (!b) // try harder
         b = tree->FindBranch(colName.c_str());

      if (!b && (tree->GetLeaf(colName.c_str()) || tree->FindLeaf(colName.c_str()))) {
         // The column name corresponds to a leaf of a branch, nothing to do
         continue;
      }

      assert(b != nullptr);
      auto *leaves = b->GetListOfLeaves();
      if (b->IsA() != TBranch::Class() || leaves->GetEntries() != 1)
         continue; // this branch is not a variable-sized array, nothing to do

      TLeaf *countLeaf = static_cast<TLeaf *>(leaves->At(0))->GetLeafCount();
      if (!countLeaf || IsStrInVec(countLeaf->GetName(), colsWithoutAliases))
         continue; // not a variable-sized array or the size branch is already there, nothing to do

      // otherwise we must insert the size in colsWithoutAliases _and_ colsWithAliases
      colsWithoutAliases.insert(colsWithoutAliases.begin() + i, countLeaf->GetName());
      colsWithAliases.insert(colsWithAliases.begin() + i, countLeaf->GetName());
      ++nCols;
      ++i; // as we inserted an element in the vector we iterate over, we need to move the index forward one extra time
   }

   return {std::move(colsWithoutAliases), std::move(colsWithAliases)};
}

void RemoveDuplicates(ColumnNames_t &columnNames)
{
   std::set<std::string> uniqueCols;
   columnNames.erase(
      std::remove_if(columnNames.begin(), columnNames.end(),
                     [&uniqueCols](const std::string &colName) { return !uniqueCols.insert(colName).second; }),
      columnNames.end());
}

void RemoveRNTupleSubFields(ColumnNames_t &columnNames)
{
   ColumnNames_t parentFields;

   std::copy_if(columnNames.cbegin(), columnNames.cend(), std::back_inserter(parentFields),
                [](const std::string &colName) { return colName.find('.') == std::string::npos; });

   columnNames.erase(std::remove_if(columnNames.begin(), columnNames.end(),
                                    [&parentFields](const std::string &colName) {
                                       if (colName.find('.') == std::string::npos)
                                          return false;
                                       const auto parentFieldName = colName.substr(0, colName.find_first_of('.'));
                                       return std::find(parentFields.cbegin(), parentFields.cend(), parentFieldName) !=
                                              parentFields.end();
                                    }),
                     columnNames.end());
}
} // namespace RDF
} // namespace Internal
} // namespace ROOT

namespace {
void AddDataSourceColumn(const std::string &colName, const std::type_info &typeID, ROOT::Detail::RDF::RLoopManager &lm,
                         ROOT::RDF::RDataSource &ds, ROOT::Internal::RDF::RColumnRegister &colRegister)
{

   if (colRegister.IsDefineOrAlias(colName))
      return;

   if (lm.HasDataSourceColumnReaders(colName, typeID))
      return;

   if (!ds.HasColumn(colName) &&
       lm.GetSuppressErrorsForMissingBranches().find(colName) == lm.GetSuppressErrorsForMissingBranches().end())
      return;

   const auto nSlots = lm.GetNSlots();
   std::vector<std::unique_ptr<ROOT::Detail::RDF::RColumnReaderBase>> colReaders;
   colReaders.reserve(nSlots);
   // TODO consider changing the interface so we return all of these for all slots in one go
   for (auto slot = 0u; slot < nSlots; ++slot)
      colReaders.emplace_back(
         ROOT::Internal::RDF::CreateColumnReader(ds, slot, colName, typeID, /*treeReader*/ nullptr));

   lm.AddDataSourceColumnReaders(colName, std::move(colReaders), typeID);
}
} // namespace

void ROOT::Internal::RDF::AddDSColumns(const std::vector<std::string> &colNames, ROOT::Detail::RDF::RLoopManager &lm,
                                       ROOT::RDF::RDataSource &ds,
                                       const std::vector<const std::type_info *> &colTypeIDs,
                                       ROOT::Internal::RDF::RColumnRegister &colRegister)
{
   auto nCols = colNames.size();
   assert(nCols == colTypeIDs.size() && "Must provide exactly one column type for each column to create");
   for (decltype(nCols) i{}; i < nCols; i++) {
      AddDataSourceColumn(colNames[i], *colTypeIDs[i], lm, ds, colRegister);
   }
}
