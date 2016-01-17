#include "Definition.h"
#include "Expression.h"
#include "Pattern.h"
#include "Context.h"

namespace {
    const Identifier matchSubjectLengthName("__match_subject_length");
    const Identifier boolTrueCaseName("true");
    const Identifier boolFalseCaseName("false");

    bool patternExpressionCreatesVariable(
        NamedEntityExpression* patternExpression,
        Context& context) {

        return !patternExpression->isReferencingStaticDataMember(context);
    }

    bool memberPatternIsIrrefutable(
        Expression* memberPattern,
        Context& context) {

        if (memberPattern == nullptr || memberPattern->isPlaceholder()) {
            return true;
        }
        if (memberPattern->isNamedEntity()) {
            NamedEntityExpression* namedEntity =
                memberPattern->cast<NamedEntityExpression>();
            if (patternExpressionCreatesVariable(namedEntity, context)) {
                // The member pattern introduces a new variable. This is an
                // irrefutable pattern.
                return true;
            }
        }
        return false;
    }

    MemberSelectorExpression* generateMatchSubjectMemberSelector(
        Expression* subject,
        Expression* memberName) {

        return new MemberSelectorExpression(subject->clone(),
                                            memberName,
                                            memberName->getLocation());
    }

    MethodCallExpression* getConstructorCall(Expression* e, Context& context) {
        if (MethodCallExpression* constructorCall =
                e->dynCast<MethodCallExpression>()) {
            constructorCall->tryResolveEnumConstructor(context);
            return constructorCall;
        } else if (NamedEntityExpression* nameExpr =
                       e->dynCast<NamedEntityExpression>()) {
            return nameExpr->getCall(context, true);
        } else if (MemberSelectorExpression* memberSelector =
                       e->dynCast<MemberSelectorExpression>()) {
            return memberSelector->getRhsCall(context);
        } else {
            return nullptr;
        }
    }

    ClassDecompositionExpression* createClassDecompositionExpr(
        MethodCallExpression* constructorCall,
        Context& context);

    ClassDecompositionExpression* createClassDecompositionFromConstructorCall(
        MethodCallExpression* constructorCall,
        Context& context) {

        ClassDecompositionExpression* classDecomposition =
            new ClassDecompositionExpression(
                Type::create(constructorCall->getName()),
                constructorCall->getLocation());

        Type* type = classDecomposition->typeCheck(context);
        ClassDefinition* classDef =
            type->getDefinition()->cast<ClassDefinition>();

        const DataMemberList& primaryCtorArgDataMembers =
            classDef->getPrimaryCtorArgDataMembers();
        const ExpressionList& constructorPatternArgs =
            constructorCall->getArguments();
        if (primaryCtorArgDataMembers.size() != constructorPatternArgs.size()) {
            Trace::error("Wrong number of arguments in constructor pattern.",
                         constructorCall);
        }

        auto j = primaryCtorArgDataMembers.cbegin();
        auto i = constructorPatternArgs.cbegin();
        while (i != constructorPatternArgs.end()) {
            Expression* patternExpr = *i;
            DataMemberDefinition* dataMember = *j;
            NamedEntityExpression* memberName =
                new NamedEntityExpression(dataMember->getName(),
                                          patternExpr->getLocation());
            if (MethodCallExpression* constructorCall =
                    getConstructorCall(patternExpr, context)) {
                patternExpr = createClassDecompositionExpr(constructorCall,
                                                           context);
            }
            classDecomposition->addMember(memberName, patternExpr);
            i++;
            j++;
        }

        return classDecomposition;
    }

    ClassDecompositionExpression* createClassDecompositionFromEnumCtorCall(
        MethodCallExpression* enumConstructorCall,
        MethodDefinition* enumConstructor,
        Context& context) {

        const ClassDefinition* enumDef = enumConstructor->getClass();

        ClassDecompositionExpression* classDecomposition =
            new ClassDecompositionExpression(
                Type::create(enumDef->getName()),
                enumConstructorCall->getLocation());

        const Identifier& enumVariantName(enumConstructor->getName());
        classDecomposition->setEnumVariantName(enumVariantName);
        classDecomposition->typeCheck(context);

        const ExpressionList& constructorPatternArgs =
            enumConstructorCall->getArguments();
        if (enumConstructor->getArgumentList().size() !=
            constructorPatternArgs.size()) {
            Trace::error(
                "Wrong number of arguments in enum constructor pattern.",
                enumConstructorCall);
        }
        if (constructorPatternArgs.empty()) {
            return classDecomposition;
        }

        ClassDefinition* enumVariantDef =
            enumDef->getNestedClass(
                Symbol::makeEnumVariantClassName(enumVariantName));
        const DataMemberList& variantDataMembers =
            enumVariantDef->getPrimaryCtorArgDataMembers();
        assert(variantDataMembers.size() == constructorPatternArgs.size());

        auto j = variantDataMembers.cbegin();
        auto i = constructorPatternArgs.cbegin();
        while (i != constructorPatternArgs.end()) {
            Expression* patternExpr = *i;
            DataMemberDefinition* dataMember = *j;
            MemberSelectorExpression* memberSelector =
                new MemberSelectorExpression(Symbol::makeEnumVariantDataName(
                                                 enumVariantName),
                                             dataMember->getName(),
                                             patternExpr->getLocation());
            if (MethodCallExpression* constructorCall =
                    getConstructorCall(patternExpr, context)) {
                patternExpr = createClassDecompositionExpr(constructorCall,
                                                           context);
            }
            classDecomposition->addMember(memberSelector, patternExpr);
            i++;
            j++;
        }

        return classDecomposition;
    }

    ClassDecompositionExpression* createClassDecompositionExpr(
        MethodCallExpression* constructorCall,
        Context& context) {

        MethodDefinition* enumConstructor =
            constructorCall->getEnumCtorMethodDefinition();
        if (enumConstructor != nullptr) {
            return createClassDecompositionFromEnumCtorCall(constructorCall,
                                                            enumConstructor,
                                                            context);
        } else {
            return createClassDecompositionFromConstructorCall(constructorCall,
                                                               context);
        }
    }
}

MatchCoverage::MatchCoverage(Type* subjectType) : notCoveredCases() {
    if (subjectType->isBoolean()) {
        notCoveredCases.insert(boolTrueCaseName);
        notCoveredCases.insert(boolFalseCaseName);
    } else if (subjectType->isEnumeration()) {
        Definition* subjectTypeDef = subjectType->getDefinition();
        assert(subjectTypeDef->isClass());
        ClassDefinition* subjectClass = subjectTypeDef->cast<ClassDefinition>();
        assert(subjectClass->isEnumeration());

        const DefinitionList& members = subjectClass->getMembers();
        for (auto i = members.cbegin(); i != members.cend(); i++) {
            Definition* member = *i;
            if (MethodDefinition* method =
                    member->dynCast<MethodDefinition>()) {
                if (method->isEnumConstructor()) {
                    notCoveredCases.insert(method->getName());
                }
            }
        }
    } else {
        notCoveredCases.insert("all");
    }
}

bool MatchCoverage::isCaseCovered(const Identifier& caseName) const {
    return (notCoveredCases.find(caseName) == notCoveredCases.end());
}

bool MatchCoverage::areAllCasesCovered() const {
    return notCoveredCases.empty();
}

void MatchCoverage::markCaseAsCovered(const Identifier& caseName) {
    notCoveredCases.erase(caseName);
}

Pattern::Pattern() : declarations(), temporaries() {}

Pattern::Pattern(const Pattern& other) : declarations(), temporaries() {
    cloneVariableDeclarations(other);
}

void Pattern::cloneVariableDeclarations(const Pattern& from) {
    Utils::cloneList(declarations, from.declarations);
    Utils::cloneList(temporaries, from.temporaries);
}

Pattern* Pattern::create(Expression* e, Context& context) {
    if (ArrayLiteralExpression* array = e->dynCast<ArrayLiteralExpression>()) {
        return new ArrayPattern(array);
    } else if (TypedExpression* typed = e->dynCast<TypedExpression>()) {
        return new TypedPattern(typed);
    } else if (ClassDecompositionExpression* classDecomposition =
                   e->dynCast<ClassDecompositionExpression>()) {
        return new ClassDecompositionPattern(classDecomposition);
    } else if (MethodCallExpression* constructorCall =
                   getConstructorCall(e, context)) {
        return new ClassDecompositionPattern(
            createClassDecompositionExpr(constructorCall, context));
    } else {
        return new SimplePattern(e);
    }
}

SimplePattern::SimplePattern(Expression* e) : expression(e) {}

SimplePattern::SimplePattern(const SimplePattern& other) :
    expression(other.expression->clone()) {}

Pattern* SimplePattern::clone() const {
    return new SimplePattern(*this);
}

bool SimplePattern::isMatchExhaustive(
    Expression* subject,
    MatchCoverage& coverage,
    bool isMatchGuardPresent,
    Context& context) {

    if (expression->isPlaceholder()) {
        return !isMatchGuardPresent;
    }

    BooleanLiteralExpression* boolLiteral =
        expression->dynCast<BooleanLiteralExpression>();
    if (boolLiteral != nullptr && subject->getType()->isBoolean()) {
        Identifier boolCaseName;
        if (boolLiteral->getValue() == true) {
            boolCaseName = boolTrueCaseName;
        } else {
            boolCaseName = boolFalseCaseName;
        }

        if (coverage.isCaseCovered(boolCaseName)) {
            Trace::error("Pattern is unreachable.", expression);
        }
        if (!isMatchGuardPresent) {
            coverage.markCaseAsCovered(boolCaseName);
            if (coverage.areAllCasesCovered()) {
                return true;
            }
        }
        return false;
    }

    NamedEntityExpression* namedEntity =
        expression->dynCast<NamedEntityExpression>();
    if (namedEntity == nullptr) {
        return false;
    }
    if (!isMatchGuardPresent) {
        if (namedEntity->isReferencingName(subject)) {
            // The pattern refers back to the subject. This is an irrefutable
            // pattern.
            return true;
        }
        if (patternExpressionCreatesVariable(namedEntity, context)) {
            // The member pattern introduces a new variable. This is an
            // irrefutable pattern.
            return true;
        }
    }
    return false;
}

BinaryExpression* SimplePattern::generateComparisonExpression(
    Expression* subject,
    Context& context) {

    const Location& location = expression->getLocation();

    NamedEntityExpression* namedEntity =
        expression->dynCast<NamedEntityExpression>();
    if (namedEntity != nullptr &&
        patternExpressionCreatesVariable(namedEntity, context)) {
        // The pattern introduces a new variable. The variable will bind to the
        // value of the match subject.
        declarations.push_back(
            new VariableDeclarationStatement(new Type(Type::Implicit),
                                             namedEntity->getIdentifier(),
                                             subject->clone(),
                                             location));
    }

    return new BinaryExpression(Operator::Equal,
                                subject->clone(),
                                expression,
                                location);
}

ArrayPattern::ArrayPattern(ArrayLiteralExpression* e) : array(e) {}

ArrayPattern::ArrayPattern(const ArrayPattern& other) :
    array(other.array->clone()) {}

Pattern* ArrayPattern::clone() const {
    return new ArrayPattern(*this);
}

bool ArrayPattern::isMatchExhaustive(
    Expression*,
    MatchCoverage&,
    bool isMatchGuardPresent,
    Context&) {

    const ExpressionList& elements = array->getElements();
    if (elements.size() == 1) {
        Expression* element = *elements.begin();
        if (element->isWildcard()) {
            return !isMatchGuardPresent;
        }
    }
    return false;
}

BinaryExpression* ArrayPattern::generateComparisonExpression(
    Expression* subject,
    Context& context) {

    BinaryExpression* comparison = generateLengthComparisonExpression();
    bool toTheRightOfWildcard = false;

    const ExpressionList& elements = array->getElements();
    for (auto i = elements.cbegin(); i != elements.cend(); i++) {
        Expression* element = *i;
        BinaryExpression* elementComparison =
            generateElementComparisonExpression(subject,
                                                i,
                                                context,
                                                toTheRightOfWildcard);
        if (elementComparison != nullptr) {
            comparison = new BinaryExpression(Operator::LogicalAnd,
                                              comparison,
                                              elementComparison,
                                              element->getLocation());
        }

        if (element->isWildcard()) {
            toTheRightOfWildcard = true;
        }
    }
    return comparison;
}

BinaryExpression* ArrayPattern::generateElementComparisonExpression(
    Expression* subject,
    ExpressionList::const_iterator i,
    Context& context,
    bool toTheRightOfWildcard) {

    Expression* element = *i;
    BinaryExpression* elementComparison = nullptr;
    switch (element->getKind()) {
        case Expression::NamedEntity:
            elementComparison =
                generateNamedEntityElementComparisonExpression(
                    subject,
                    i,
                    context,
                    toTheRightOfWildcard);
            break;
        case Expression::Placeholder:
        case Expression::Wildcard:
            break;
        default:
            elementComparison = new BinaryExpression(
                Operator::Equal,
                generateArraySubscriptExpression(subject,
                                                 i,
                                                 toTheRightOfWildcard),
                element,
                element->getLocation());
            break;
    }
    return elementComparison;
}

BinaryExpression* ArrayPattern::generateNamedEntityElementComparisonExpression(
    Expression* subject,
    ExpressionList::const_iterator i,
    Context& context,
    bool toTheRightOfWildcard) {

    Expression* element = *i;
    NamedEntityExpression* namedEntity =
        element->dynCast<NamedEntityExpression>();
    assert(namedEntity != nullptr);

    if (patternExpressionCreatesVariable(namedEntity, context)) {
        // The pattern introduces a new variable. The variable will bind to the
        // value of the corresponding array element in the match subject.
        const Location& location = namedEntity->getLocation();
        Expression* matchSubjectElementExpression =
            generateArraySubscriptExpression(subject, i, toTheRightOfWildcard);
        declarations.push_back(
            new VariableDeclarationStatement(new Type(Type::Implicit),
                                             namedEntity->getIdentifier(),
                                             matchSubjectElementExpression,
                                             location));
        return nullptr;
    }
    return new BinaryExpression(
        Operator::Equal,
        generateArraySubscriptExpression(subject, i, toTheRightOfWildcard),
        element,
        element->getLocation());
}

ArraySubscriptExpression* ArrayPattern::generateArraySubscriptExpression(
    Expression* subject,
    ExpressionList::const_iterator i,
    bool toTheRightOfWildcard) {

    Expression* element = *i;
    const Location& location = element->getLocation();
    Expression* indexExpression = nullptr;
    const ExpressionList& elements = array->getElements();
    if (toTheRightOfWildcard) {
        int reverseIndex = std::distance(i, elements.end());
        indexExpression =
            new BinaryExpression(Operator::Subtraction,
                                 new NamedEntityExpression(
                                     matchSubjectLengthName,
                                     location),
                                 new IntegerLiteralExpression(reverseIndex,
                                                              location),
                                 location);
    } else {
        indexExpression =
            new IntegerLiteralExpression(std::distance(elements.begin(), i),
                                         location);
    }
    return new ArraySubscriptExpression(subject->clone(), indexExpression);
}

BinaryExpression* ArrayPattern::generateLengthComparisonExpression() {
    int numberOfElements = 0;
    bool wildcardPresent = false;

    const ExpressionList& elements = array->getElements();
    for (auto i = elements.cbegin(); i != elements.cend(); i++) {
        Expression* element = *i;
        if (element->isWildcard()) {
            if (wildcardPresent) {
                Trace::error("Wilcard '..' can only be present once in an array"
                             " pattern.",
                             element);
            }
            wildcardPresent = true;
        } else {
            numberOfElements++;
        }
    }

    Operator::Kind op;
    if (wildcardPresent) {
        op = Operator::GreaterOrEqual;
    } else {
        op = Operator::Equal;
    }

    const Location& location = array->getLocation();
    return
        new BinaryExpression(op,
                             new NamedEntityExpression(matchSubjectLengthName,
                                                       location),
                             new IntegerLiteralExpression(numberOfElements,
                                                          location),
                             location);
}

VariableDeclarationStatement*
ArrayPattern::generateMatchSubjectLengthDeclaration(Expression* subject) {
    const Location& location = subject->getLocation();
    MemberSelectorExpression* arrayLengthSelector =
        new MemberSelectorExpression(
            subject->clone(),
            new NamedEntityExpression(BuiltInTypes::arrayLengthMethodName,
                                      location),
            location);
    return new VariableDeclarationStatement(new Type(Type::Integer),
                                            matchSubjectLengthName,
                                            arrayLengthSelector,
                                            location);
}

ClassDecompositionPattern::ClassDecompositionPattern(
    ClassDecompositionExpression* e) :
    classDecomposition(e) {}

ClassDecompositionPattern::ClassDecompositionPattern(
    const ClassDecompositionPattern& other) :
    classDecomposition(other.classDecomposition->clone()) {}

Pattern* ClassDecompositionPattern::clone() const {
    return new ClassDecompositionPattern(*this);
}

bool ClassDecompositionPattern::isMatchExhaustive(
    Expression* subject,
    MatchCoverage& coverage,
    bool isMatchGuardPresent,
    Context& context) {

    Type* classPatternType = classDecomposition->typeCheck(context);
    const Identifier& enumVariantName =
        classDecomposition->getEnumVariantName();

    if (!enumVariantName.empty()) {
        return isEnumMatchExhaustive(enumVariantName,
                                     subject,
                                     coverage,
                                     isMatchGuardPresent,
                                     classPatternType,
                                     context);
    }

    if (!Type::areEqualNoConstCheck(subject->getType(),
                                    classPatternType,
                                    false)) {
        return false;
    }

    if (!isMatchGuardPresent && areAllMemberPatternsIrrefutable(context)) {
        return true;
    }
    return false;
}

bool ClassDecompositionPattern::isEnumMatchExhaustive(
    const Identifier& enumVariantName,
    Expression* subject,
    MatchCoverage& coverage,
    bool isMatchGuardPresent,
    Type* patternType,
    Context& context) {

    if (!Type::areEqualNoConstCheck(subject->getType(), patternType, false)) {
        Trace::error("Enum type in pattern must be the same as the match "
                     "subject type. Pattern type: " +
                     patternType->toString() +
                     ". Match subject type: " +
                     subject->getType()->toString(),
                     classDecomposition);
    }

    if (coverage.isCaseCovered(enumVariantName)) {
        Trace::error("Pattern is unreachable.", classDecomposition);
    }
    if (!isMatchGuardPresent && areAllMemberPatternsIrrefutable(context)) {
        coverage.markCaseAsCovered(enumVariantName);
        if (coverage.areAllCasesCovered()) {
            return true;
        }
    }
    return false;
}

bool ClassDecompositionPattern::areAllMemberPatternsIrrefutable(
    Context& context) {

    const ClassDecompositionExpression::MemberList& members =
        classDecomposition->getMembers();
    for (auto i = members.cbegin(); i != members.cend(); i++) {
        const ClassDecompositionExpression::Member& member = *i;
        if (!memberPatternIsIrrefutable(member.patternExpr, context)) {
            return false;
        }
    }
    return true;
}

BinaryExpression* ClassDecompositionPattern::generateComparisonExpression(
    Expression* subject,
    Context& context) {

    BinaryExpression* comparison = generateTypeComparisonExpression(&subject);

    const ClassDecompositionExpression::MemberList& members =
        classDecomposition->getMembers();
    for (auto i = members.cbegin(); i != members.cend(); i++) {
        const ClassDecompositionExpression::Member& member = *i;
        if (memberPatternIsIrrefutable(member.patternExpr, context)) {
            generateVariableCreatedByMemberPattern(member, subject, context);
        } else {
            BinaryExpression* memberComparison =
                generateMemberComparisonExpression(subject, member, context);
            if (comparison == nullptr) {
                comparison = memberComparison;
            } else {
                comparison =
                    new BinaryExpression(Operator::LogicalAnd,
                                         comparison,
                                         memberComparison,
                                         member.patternExpr->getLocation());
            }
        }
    }
    return comparison;
}

void ClassDecompositionPattern::generateVariableCreatedByMemberPattern(
    const ClassDecompositionExpression::Member& member,
    Expression* subject,
    Context& context) {

    NamedEntityExpression* patternVar = nullptr;
    if (member.patternExpr == nullptr) {
        patternVar = member.nameExpr->dynCast<NamedEntityExpression>();
    } else {
        patternVar = member.patternExpr->dynCast<NamedEntityExpression>();
        if (patternVar != nullptr &&
            !patternExpressionCreatesVariable(patternVar, context)) {
            patternVar = nullptr;
        }
    }

    if (patternVar != nullptr) {
        Expression* matchSubjectMemberExpression =
            generateMatchSubjectMemberSelector(subject, member.nameExpr);
        declarations.push_back(
            new VariableDeclarationStatement(new Type(Type::Implicit),
                                             patternVar->getIdentifier(),
                                             matchSubjectMemberExpression,
                                             patternVar->getLocation()));
    }
}

BinaryExpression* ClassDecompositionPattern::generateMemberComparisonExpression(
    Expression* subject,
    const ClassDecompositionExpression::Member& member,
    Context& context) {

    BinaryExpression* comparisonExpression = nullptr;
    Expression* subjectMemberSelector =
        generateMatchSubjectMemberSelector(subject, member.nameExpr);
    Expression* patternExpr = member.patternExpr;
    if (ClassDecompositionExpression* classDecompositionExpr =
            patternExpr->dynCast<ClassDecompositionExpression>()) {
        ClassDecompositionPattern* classDecompositionPattern =
            new ClassDecompositionPattern(classDecompositionExpr);

        // The type of the subject expression needs to be calculated before
        // calling generateComparisonExpression().
        Context tmpContext(context);
        subjectMemberSelector = subjectMemberSelector->transform(tmpContext);
        subjectMemberSelector->typeCheck(tmpContext);

        comparisonExpression =
            classDecompositionPattern->generateComparisonExpression(
                subjectMemberSelector,
                context);
        cloneVariableDeclarations(*classDecompositionPattern);
    } else {
        comparisonExpression = new BinaryExpression(Operator::Equal,
                                                    subjectMemberSelector,
                                                    patternExpr,
                                                    patternExpr->getLocation());
    }
    return comparisonExpression;
}

BinaryExpression* ClassDecompositionPattern::generateTypeComparisonExpression(
    Expression** subject) {

    Expression* originalSubject = *subject;
    const Identifier& enumVariantName =
        classDecomposition->getEnumVariantName();
    if (!enumVariantName.empty()) {
        return generateEnumVariantTagComparisonExpression(originalSubject,
                                                          enumVariantName);
    }

    Type* classDecompositionType = classDecomposition->getType();
    if (Type::areEqualNoConstCheck(originalSubject->getType(),
                                   classDecompositionType,
                                   false)) {
        // No need to generate type comparison. The pattern type and subject
        // types are equal.
        return nullptr;
    }

    const Location& location = classDecomposition->getLocation();
    const Identifier
        castedSubjectName("__" + classDecompositionType->getName() + "_" +
                          originalSubject->generateVariableName());
    Type* castedSubjectType = classDecompositionType->clone();
    castedSubjectType->setConstant(false);
    temporaries.push_back(new VariableDeclarationStatement(castedSubjectType,
                                                           castedSubjectName,
                                                           nullptr,
                                                           location));
    TypeCastExpression* typeCast =
        new TypeCastExpression(castedSubjectType,
                               originalSubject->clone(),
                               location);
    Expression* castedSubject = new LocalVariableExpression(castedSubjectType,
                                                            castedSubjectName,
                                                            location);
    *subject = castedSubject;
    return new BinaryExpression(
        Operator::NotEqual,
        new BinaryExpression(Operator::AssignmentExpression,
                             castedSubject->clone(),
                             typeCast,
                             location),
        new NullExpression(location),
        location);
}

BinaryExpression*
ClassDecompositionPattern::generateEnumVariantTagComparisonExpression(
    Expression* subject,
    const Identifier& enumVariantName) {

    const Location& location = classDecomposition->getLocation();
    const Identifier& enumName = subject->getType()->getFullConstructedName();

    MemberSelectorExpression* tagMember =
        new MemberSelectorExpression(subject->clone(),
                                     new NamedEntityExpression(
                                         CommonNames::enumTagVariableName,
                                         location),
                                     location);
    MemberSelectorExpression* tagConstant =
        new MemberSelectorExpression(new NamedEntityExpression(enumName,
                                                               location),
                                     new NamedEntityExpression(
                                         Symbol::makeEnumVariantTagName(
                                             enumVariantName),
                                         location),
                                     location);
    return new BinaryExpression(Operator::Equal,
                                tagMember,
                                tagConstant,
                                location);
}

TypedPattern::TypedPattern(TypedExpression* e) : typedExpression(e) {}

TypedPattern::TypedPattern(const TypedPattern& other) :
    typedExpression(other.typedExpression->clone()) {}

Pattern* TypedPattern::clone() const {
    return new TypedPattern(*this);
}

bool TypedPattern::isMatchExhaustive(
    Expression* subject,
    MatchCoverage&,
    bool isMatchGuardPresent,
    Context& context) {

    Type* targetType = typedExpression->typeCheck(context);
    if (Type::areEqualNoConstCheck(subject->getType(), targetType, false) &&
        !isMatchGuardPresent) {
        return true;
    }

    return false;
}

BinaryExpression* TypedPattern::generateComparisonExpression(
    Expression* subject,
    Context&) {

    Type* targetType = typedExpression->getType();
    const Location& location = typedExpression->getLocation();
    const Identifier
        castedSubjectName("__" + targetType->getName() + "_" +
                          subject->generateVariableName());
    Type* castedSubjectType = targetType->clone();
    castedSubjectType->setConstant(false);
    temporaries.push_back(new VariableDeclarationStatement(castedSubjectType,
                                                           castedSubjectName,
                                                           nullptr,
                                                           location));
    TypeCastExpression* typeCast =
        new TypeCastExpression(castedSubjectType, subject->clone(), location);
    Expression* castedSubject = new LocalVariableExpression(castedSubjectType,
                                                            castedSubjectName,
                                                            location);
    if (NamedEntityExpression* resultName =
            typedExpression->getResultName()->dynCast<
                NamedEntityExpression>()) {
        declarations.push_back(
            new VariableDeclarationStatement(new Type(Type::Implicit),
                                             resultName->getIdentifier(),
                                             castedSubject->clone(),
                                             resultName->getLocation()));
    }
    return new BinaryExpression(
        Operator::NotEqual,
        new BinaryExpression(Operator::AssignmentExpression,
                             castedSubject->clone(),
                             typeCast,
                             location),
        new NullExpression(location),
        location);
}
