#include "Qt4ConstraintMacro.h"
#include <iostream>
#include <sstream>

using namespace action_authoring;
using namespace std;

Qt4ConstraintMacro::
Qt4ConstraintMacro(ConstraintMacroPtr constraint, int constraintIndex) :
    _gui_panel(new TogglePanel(this, "")),
    _gui_name(new QLineEdit()),
    _gui_robotJointType(new QComboBox()),
    _gui_constraintType(new QComboBox()),
    _gui_affordanceType(new QComboBox()),
    _gui_time_lower_bound(new QDoubleSpinBox()),
    _gui_time_upper_bound(new QDoubleSpinBox())
{
    // constructor
    _constraint = constraint;
    _gui_time_lower_bound->setSuffix(" sec");
    _gui_time_upper_bound->setSuffix(" sec");
    _initialized = false;
    _constraintIndex = constraintIndex;
    _updatingElementsFromState = false;
}

Qt4ConstraintMacro::
~Qt4ConstraintMacro()
{
    //TODO; hack
    //    std::cout << "destructive destructor destructing" << std::endl;
    _gui_panel->setParent(NULL);
    //delete _gui_panel;
}

std::string
Qt4ConstraintMacro::
getSelectedLinkName()
{
    return _gui_robotJointType->currentText().toStdString();
}

void
Qt4ConstraintMacro::
setConstraintIndex(int constraintIndex)
{
    _constraintIndex = constraintIndex;
    updateElementsFromState();
}

bool
Qt4ConstraintMacro::
isInitialized()
{
    return _initialized;
}

TogglePanel *
Qt4ConstraintMacro::
getPanel()
{
    //    QString waypointTitle = QString::fromStdString(_constraint->getName());
    //    std::cout << "title is " << _constraint->getName() << std::endl;
    if (_initialized)
    {
        return _gui_panel;
    }

    QGroupBox *groupBox = new QGroupBox();
    QPushButton *editButton = new QPushButton(QString::fromUtf8("edit"));

    // update the global maps
    //_all_panels[waypoint_constraint->getName()] = outputPanel;
    //_all_robot_link_combos[waypoint_constraint->getName()] = radio1;

    // See RelationState.h for enum definitions
    for (uint i = 0; i < RelationState::RELATION_TYPE_LENGTH; i++) {
        // insert backwards
        RelationState::bm_type::right_const_iterator right_iter = 
            RelationState::rNameToValue.right.find((RelationState::RelationType)(RelationState::RELATION_TYPE_LENGTH - 1 - i));
        _gui_constraintType->insertItem(0, QString::fromStdString(right_iter->second));
    }

    /*
    //_gui_constraintType->insertItem(0, "near");
    //_gui_constraintType->insertItem(0, "surface touches");
    //_gui_constraintType->insertItem(0, "tangent to");
    _gui_constraintType->insertItem(0, "3D Offset"); // OFFSET
    //_gui_constraintType->insertItem(0, "point to point");
    //_gui_constraintType->insertItem(0, "force closure");
    //_gui_constraintType->insertItem(0, "grasps");
    //_gui_constraintType->insertItem(0, "undefined");
    */

    QVBoxLayout *vbox = new QVBoxLayout;
    QHBoxLayout *top_line_hbox = new QHBoxLayout();
    QWidget *top_line_container = new QWidget();
    top_line_hbox->addWidget(_gui_name);
    top_line_hbox->addWidget(new QLabel("start time: "));
    top_line_hbox->addWidget(_gui_time_lower_bound);
    top_line_hbox->addWidget(new QLabel("end time: "));
    top_line_hbox->addWidget(_gui_time_upper_bound);
    //    top_line_hbox->addWidget(new QPushButton("click to bind"));
    top_line_container->setLayout(top_line_hbox);
    vbox->addWidget(top_line_container);

    QHBoxLayout *hbox = new QHBoxLayout;
    hbox->addWidget(new QLabel("robot"));
    hbox->addWidget(_gui_robotJointType);
    hbox->addWidget(new QLabel("relation"));
    hbox->addWidget(_gui_constraintType);
    hbox->addWidget(new QLabel("affordance"));
    hbox->addWidget(_gui_affordanceType);
    hbox->addWidget(editButton);
    hbox->addStretch(1);
    QWidget *boxcontainer = new QWidget();
    boxcontainer->setLayout(hbox);
    vbox->addWidget(boxcontainer);
    groupBox->setLayout(vbox);

    // MUST go before the QT connections have been made
    updateElementsFromState();

    connect(_gui_time_lower_bound, SIGNAL(valueChanged(double)), this, SLOT(updateStateFromElements()));
    connect(_gui_time_upper_bound, SIGNAL(valueChanged(double)), this, SLOT(updateStateFromElements()));

    connect(_gui_name, SIGNAL(textChanged(QString)), this, SLOT(updateStateFromElements()));
    connect(_gui_robotJointType, SIGNAL(currentIndexChanged(int)), this, SLOT(updateStateFromElements()));
    connect(_gui_constraintType, SIGNAL(currentIndexChanged(int)), this, SLOT(updateStateFromElements()));
    connect(_gui_affordanceType, SIGNAL(currentIndexChanged(int)), this, SLOT(updateStateFromElements()));
    connect(editButton, SIGNAL(released()), this, SLOT(setActive()));

    _gui_panel->addWidget(groupBox);
    _initialized = true;
    return _gui_panel;
}

void
Qt4ConstraintMacro::
setModelObjects(std::vector<affordance::AffConstPtr> &affordances,
                std::vector<affordance::ManipulatorStateConstPtr> &manipulators)
{
    _affordances = affordances;
    _manipulators = manipulators;
    updateElementsFromState();
}

ConstraintMacroPtr
Qt4ConstraintMacro::
getConstraintMacro()
{
    return _constraint;
}

// todo; very primitive; need affordance UID type
void
Qt4ConstraintMacro::
updateStateFromElements()
{
    if (_updatingElementsFromState)
    {
        return;
    }

    _constraint->setName(_gui_name->text().toStdString());
    _gui_panel->setTitle(QString("[%1] ").arg(_constraintIndex) + QString::fromStdString(_constraint->getName()));

    _constraint->setTimeLowerBound(_gui_time_lower_bound->value());
    _constraint->setTimeUpperBound(_gui_time_upper_bound->value());
    
    if (_gui_constraintType->currentIndex() != (int)(_constraint->getAtomicConstraint()->getRelationState()->getRelationType()) &&
        _gui_constraintType->currentIndex() >= 0 && _gui_constraintType->currentIndex() < RelationState::RELATION_TYPE_LENGTH)
    {
        std::cout << "made new pointer, index" << _gui_constraintType->currentIndex() << std::endl;
        // TODO refractor into factory
        RelationState::RelationType rType = (RelationState::RelationType)_gui_constraintType->currentIndex();
        if (rType == RelationState::OFFSET) 
        {
            _constraint->getAtomicConstraint()->setRelationState((OffsetRelationPtr)new OffsetRelation());
        }
        else if (rType == RelationState::POINT_CONTACT)
        {
            _constraint->getAtomicConstraint()->setRelationState((PointContactRelationPtr)new PointContactRelation());
        }
        else if (rType == RelationState::NOT_IN_CONTACT)
        {
          _constraint->getAtomicConstraint()->setRelationState((RelationStatePtr)new RelationState(RelationState::NOT_IN_CONTACT));
        }
        else
          {
            _constraint->getAtomicConstraint()->setRelationState((RelationStatePtr)new RelationState(RelationState::UNDEFINED));
          }
        std::cout << "set new pointer" << std::endl;
    }

    if (_gui_robotJointType->currentIndex() >= 0)
    {
        _constraint->getAtomicConstraint()->setManipulator(
                           _manipulators[_gui_robotJointType->currentIndex()]->getGlobalUniqueId());
    }

    if (_gui_affordanceType->currentIndex() >= 0)
    {
        _constraint->getAtomicConstraint()->setAffordance(
                 _affordances[_gui_affordanceType->currentIndex()]->getGlobalUniqueId());
    }

    setActive();
}


void
Qt4ConstraintMacro::
setActiveExternal()
{
    setActive();
}


void
Qt4ConstraintMacro::
setActive()
{
    emit activatedSignal(this);
}

void
Qt4ConstraintMacro::
setSelected(bool selected)
{
    _gui_panel->setSelected(selected);
}

void
Qt4ConstraintMacro::
updateElementsFromState()
{
    std::cout << "Updating elements from state..." << std::endl;

    // if we don't block signals, we willl overwrite state when we are
    // rebuilding the GUI elements because the values will change and
    // automatically trigger state updates
    _updatingElementsFromState = true;
    _gui_robotJointType->blockSignals(true);
    _gui_affordanceType->blockSignals(true);
    _gui_constraintType->blockSignals(true);

    _gui_name->setText(QString::fromStdString(_constraint->getName()));
    _gui_panel->setTitle(QString("[%1] ").arg(_constraintIndex) + QString::fromStdString(_constraint->getName()));

    _gui_robotJointType->clear();

    _gui_time_lower_bound->setValue(_constraint->getTimeLowerBound());
    _gui_time_upper_bound->setValue(_constraint->getTimeUpperBound());

    // assumes that the enum maps to the correct items
    _gui_constraintType->setCurrentIndex((int)_constraint->getAtomicConstraint()->getRelationState()->getRelationType());

    // re-initialize the maps
    _affordance1IndexMap.clear();
    _affordance2IndexMap.clear();

    if (_manipulators.size() > 0)
    {
        // update the left side combo box
        _gui_robotJointType->clear();

        for (int i = 0; i < (int)_manipulators.size(); i++)
        {
            _gui_robotJointType->insertItem(i, QString::fromStdString(_manipulators[i]->getName()));
            _affordance1IndexMap[_manipulators[i]->getGlobalUniqueId()] = i;
        }

        // select the correct joint name
        std::map<affordance::GlobalUID, int>::const_iterator it = _affordance1IndexMap.find(
                    _constraint->getAtomicConstraint()->getManipulator()->getGlobalUniqueId());

        if (it != _affordance1IndexMap.end())
        {
            _gui_robotJointType->setCurrentIndex(it->second);
            //std::cout << "found LH affordance iterator: " << it->second << std::endl;
        }
    }

    if (_affordances.size() > 0)
    {
        // update the right side combo box
        _gui_affordanceType->clear();

        for (int i = 0; i < (int)_affordances.size(); i++)
        {
            if (_affordances[i] != NULL)
            {
                _gui_affordanceType->insertItem(i, QString::fromStdString(_affordances[i]->getName()));
                _affordance2IndexMap[_affordances[i]->getGlobalUniqueId()] = i;
            }
            else
            {
                std::cout << "affordance " << i << " was NULL" << std::endl;
                // todo: throw exception
            }
        }

        // select the current affordance
        std::map<affordance::GlobalUID, int>::const_iterator it2 = _affordance2IndexMap.find(
                    _constraint->getAtomicConstraint()->getAffordance()->getGlobalUniqueId());

        if (it2 != _affordance2IndexMap.end())
        {
            _gui_affordanceType->setCurrentIndex(it2->second);
            //	    std::cout << "found RH index ((" << it2->second << ")): " << " for GUID " <<
            //		_constraint->getAtomicConstraint()->getAffordance()->getGUIDAsString() << std::endl;
        }
    }

    _gui_robotJointType->blockSignals(false);
    _gui_affordanceType->blockSignals(false);
    _gui_constraintType->blockSignals(false);
    _updatingElementsFromState = false;
}