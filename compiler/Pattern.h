#ifndef Pattern_h
#define Pattern_h

#include <set>

#include "CommonTypes.h"
#include "Type.h"
#include "Statement.h"

class Expression;
class BinaryExpression;
class ArrayLiteralExpression;
class ArraySubscriptExpression;
class ClassDecompositionExpression;
class MemberSelectorExpression;
class TypedExpression;

class MatchCoverage {
public:
    MatchCoverage(Type* subjectType);

    bool isCaseCovered(const Identifier& caseName) const;
    bool areAllCasesCovered() const;
    void markCaseAsCovered(const Identifier& caseName);

private:
    typedef std::set<Identifier> CaseSet;

    CaseSet notCoveredCases;
};

class Pattern {
public:
    Pattern();
    Pattern(const Pattern& other);
    virtual ~Pattern() {}

    static Pattern* create(Expression* expression, Context& context);

    virtual Pattern* clone() const = 0;
    virtual bool isMatchExhaustive(
        Expression* subject,
        MatchCoverage& coverage,
        bool isMatchGuardPresent,
        Context& context) = 0;
    virtual BinaryExpression* generateComparisonExpression(
        Expression* subject,
        Context& context) = 0;

    const VariableDeclarationStatementList&
    getVariablesCreatedByPattern() const {
        return declarations;
    }

    const VariableDeclarationStatementList&
    getTemporariesCreatedByPattern() const {
        return temporaries;
    }

protected:
    void cloneVariableDeclarations(const Pattern& from);

    VariableDeclarationStatementList declarations;
    VariableDeclarationStatementList temporaries;
};

class SimplePattern: public Pattern {
public:
    explicit SimplePattern(Expression* e);
    SimplePattern(const SimplePattern& other);

    virtual Pattern* clone() const;
    virtual bool isMatchExhaustive(
        Expression* subject,
        MatchCoverage& coverage,
        bool isMatchGuardPresent,
        Context& context);
    virtual BinaryExpression* generateComparisonExpression(
        Expression* subject,
        Context& context);

private:
    Expression* expression;
};

class ArrayPattern: public Pattern {
public:
    explicit ArrayPattern(ArrayLiteralExpression* e);
    ArrayPattern(const ArrayPattern& other);

    virtual Pattern* clone() const;
    virtual bool isMatchExhaustive(
        Expression* subject,
        MatchCoverage& coverage,
        bool isMatchGuardPresent,
        Context& context);
    virtual BinaryExpression* generateComparisonExpression(
        Expression* subject,
        Context& context);

    static VariableDeclarationStatement*
    generateMatchSubjectLengthDeclaration(Expression* subject);

private:
    BinaryExpression* generateElementComparisonExpression(
        Expression* subject,
        ExpressionList::const_iterator i,
        Context& context,
        bool toTheRightOfWildcard);
    BinaryExpression* generateNamedEntityElementComparisonExpression(
        Expression* subject,
        ExpressionList::const_iterator i,
        Context& context,
        bool toTheRightOfWildcard);
    ArraySubscriptExpression* generateArraySubscriptExpression(
        Expression* subject,
        ExpressionList::const_iterator i,
        bool toTheRightOfWildcard);
    BinaryExpression* generateLengthComparisonExpression();

    ArrayLiteralExpression* array;
};

class ClassDecompositionPattern: public Pattern {
public:
    explicit ClassDecompositionPattern(ClassDecompositionExpression* e);
    ClassDecompositionPattern(const ClassDecompositionPattern& other);

    virtual Pattern* clone() const;
    virtual bool isMatchExhaustive(
        Expression* subject,
        MatchCoverage& coverage,
        bool isMatchGuardPresent,
        Context& context);
    virtual BinaryExpression* generateComparisonExpression(
        Expression* subject,
        Context& context);

private:
    bool isEnumMatchExhaustive(
        const Identifier& enumVariantName,
        Expression* subject,
        MatchCoverage& coverage,
        bool isMatchGuardPresent,
        Type* patternType,
        Context& context);
    bool areAllMemberPatternsIrrefutable(Context& context);
    void generateVariableCreatedByMemberPattern(
        const ClassDecompositionExpression::Member& member,
        Expression* subject,
        Context& context);
    BinaryExpression* generateMemberComparisonExpression(
        Expression* subject,
        const ClassDecompositionExpression::Member& member,
        Context& context);
    BinaryExpression* generateTypeComparisonExpression(Expression** subject);
    BinaryExpression* generateEnumVariantTagComparisonExpression(
        Expression* subject,
        const Identifier& enumVariantName);

    ClassDecompositionExpression* classDecomposition;
};

class TypedPattern: public Pattern {
public:
    explicit TypedPattern(TypedExpression* e);
    TypedPattern(const TypedPattern& other);

    virtual Pattern* clone() const;
    virtual bool isMatchExhaustive(
        Expression* subject,
        MatchCoverage& coverage,
        bool isMatchGuardPresent,
        Context& context);
    virtual BinaryExpression* generateComparisonExpression(
        Expression* subject,
        Context& context);

private:
    TypedExpression* typedExpression;
};

#endif
