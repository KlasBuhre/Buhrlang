#ifndef Pattern_h
#define Pattern_h

#include <set>

#include "CommonTypes.h"
#include "Type.h"
#include "Statement.h"
#include "Expression.h"

class MatchCoverage {
public:
    explicit MatchCoverage(const Type* subjectType);

    bool isCaseCovered(const Identifier& caseName) const;
    bool areAllCasesCovered() const;
    void markCaseAsCovered(const Identifier& caseName);

private:
    using CaseSet = std::set<Identifier>;

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
        const Expression* subject,
        MatchCoverage& coverage,
        bool isMatchGuardPresent,
        Context& context) = 0;
    virtual BinaryExpression* generateComparisonExpression(
        const Expression* subject,
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

    Pattern* clone() const override;
    bool isMatchExhaustive(
        const Expression* subject,
        MatchCoverage& coverage,
        bool isMatchGuardPresent,
        Context& context) override;
    BinaryExpression* generateComparisonExpression(
        const Expression* subject,
        Context& context) override;

private:
    Expression* expression;
};

class ArrayPattern: public Pattern {
public:
    explicit ArrayPattern(ArrayLiteralExpression* e);
    ArrayPattern(const ArrayPattern& other);

    Pattern* clone() const override;
    bool isMatchExhaustive(
        const Expression* subject,
        MatchCoverage& coverage,
        bool isMatchGuardPresent,
        Context& context) override;
    BinaryExpression* generateComparisonExpression(
        const Expression* subject,
        Context& context) override;

    static VariableDeclarationStatement*
    generateMatchSubjectLengthDeclaration(const Expression* subject);

private:
    BinaryExpression* generateElementComparisonExpression(
        const Expression* subject,
        ExpressionList::const_iterator i,
        Context& context,
        bool toTheRightOfWildcard);
    BinaryExpression* generateNamedEntityElementComparisonExpression(
        const Expression* subject,
        ExpressionList::const_iterator i,
        Context& context,
        bool toTheRightOfWildcard);
    ArraySubscriptExpression* generateArraySubscriptExpression(
        const Expression* subject,
        ExpressionList::const_iterator i,
        bool toTheRightOfWildcard);
    BinaryExpression* generateLengthComparisonExpression();

    ArrayLiteralExpression* array;
};

class ClassDecompositionPattern: public Pattern {
public:
    explicit ClassDecompositionPattern(ClassDecompositionExpression* e);
    ClassDecompositionPattern(const ClassDecompositionPattern& other);

    Pattern* clone() const override;
    bool isMatchExhaustive(
        const Expression* subject,
        MatchCoverage& coverage,
        bool isMatchGuardPresent,
        Context& context) override;
    BinaryExpression* generateComparisonExpression(
        const Expression* subject,
        Context& context) override;

private:
    bool isEnumMatchExhaustive(
        const Identifier& enumVariantName,
        const Expression* subject,
        MatchCoverage& coverage,
        bool isMatchGuardPresent,
        Type* patternType,
        Context& context);
    bool areAllMemberPatternsIrrefutable(Context& context);
    void generateVariableCreatedByMemberPattern(
        const ClassDecompositionExpression::Member& member,
        const Expression* subject,
        Context& context);
    BinaryExpression* generateMemberComparisonExpression(
        const Expression* subject,
        const ClassDecompositionExpression::Member& member,
        Context& context);
    BinaryExpression* generateTypeComparisonExpression(
        const Expression** subject);
    BinaryExpression* generateEnumVariantTagComparisonExpression(
        const Expression* subject,
        const Identifier& enumVariantName);

    ClassDecompositionExpression* classDecomposition;
};

class TypedPattern: public Pattern {
public:
    explicit TypedPattern(TypedExpression* e);
    TypedPattern(const TypedPattern& other);

    Pattern* clone() const override;
    bool isMatchExhaustive(
        const Expression* subject,
        MatchCoverage& coverage,
        bool isMatchGuardPresent,
        Context& context) override;
    BinaryExpression* generateComparisonExpression(
        const Expression* subject,
        Context& context) override;

private:
    TypedExpression* typedExpression;
};

#endif
