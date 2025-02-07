// @HEADER
// *****************************************************************************
//           Panzer: A partial differential equation assembly
//       engine for strongly coupled complex multiphysics systems
//
// Copyright 2011 NTESS and the Panzer contributors.
// SPDX-License-Identifier: BSD-3-Clause
// *****************************************************************************
// @HEADER

///////////////////////////////////////////////////////////////////////////////
//
//  Include Files
//
///////////////////////////////////////////////////////////////////////////////

// C++
#include <cstdio>
#include <string>
#include <vector>

// Kokkos
#include "Kokkos_View_Fad.hpp"

// Panzer
#include "PanzerAdaptersSTK_config.hpp"
#include "Panzer_BasisIRLayout.hpp"
#include "Panzer_BlockedTpetraLinearObjFactory.hpp"
#include "Panzer_BlockedDOFManager.hpp"
#include "Panzer_DOFManager.hpp"
#include "Panzer_Evaluator_WithBaseImpl.hpp"
#include "Panzer_FieldManagerBuilder.hpp"
#include "Panzer_GatherOrientation.hpp"
#include "Panzer_PureBasis.hpp"
#include "Panzer_STKConnManager.hpp"
#include "Panzer_STK_Interface.hpp"
#include "Panzer_STK_SetupUtilities.hpp"
#include "Panzer_STK_SquareQuadMeshFactory.hpp"
#include "Panzer_STK_Version.hpp"
#include "Panzer_Workset.hpp"
#include "Panzer_LOCPair_GlobalEvaluationData.hpp"
#include "Panzer_GlobalEvaluationDataContainer.hpp"

// Teuchos
#include "Teuchos_DefaultMpiComm.hpp"
#include "Teuchos_GlobalMPISession.hpp"
#include "Teuchos_OpaqueWrapper.hpp"
#include "Teuchos_RCP.hpp"
#include "Teuchos_TimeMonitor.hpp"
#include "Teuchos_UnitTestHarness.hpp"

// Thyra
#include "Thyra_ProductVectorBase.hpp"
#include "Thyra_VectorStdOps.hpp"

// Tpetra
#include "Tpetra_Vector.hpp"

// user_app
#include "user_app_EquationSetFactory.hpp"

typedef double ScalarT;
using LocalOrdinalT = panzer::LocalOrdinal;
using GlobalOrdinalT = panzer::GlobalOrdinal;

typedef Tpetra::Vector<ScalarT, LocalOrdinalT, GlobalOrdinalT> VectorType;
typedef Tpetra::Operator<ScalarT, LocalOrdinalT, GlobalOrdinalT> OperatorType;
typedef Tpetra::CrsMatrix<ScalarT, LocalOrdinalT, GlobalOrdinalT> CrsMatrixType;
typedef Tpetra::CrsGraph<LocalOrdinalT, GlobalOrdinalT> CrsGraphType;
typedef Tpetra::Map<LocalOrdinalT, GlobalOrdinalT> MapType;
typedef Tpetra::Import<LocalOrdinalT, GlobalOrdinalT> ImportType;
typedef Tpetra::Export<LocalOrdinalT, GlobalOrdinalT> ExportType;

typedef Thyra::TpetraLinearOp<ScalarT, LocalOrdinalT, GlobalOrdinalT> ThyraLinearOp;

typedef panzer::BlockedTpetraLinearObjFactory<panzer::Traits, ScalarT, LocalOrdinalT, GlobalOrdinalT> BlockedTpetraLinObjFactoryType;
typedef panzer::TpetraLinearObjFactory<panzer::Traits, ScalarT, LocalOrdinalT, GlobalOrdinalT> TpetraLinObjFactoryType;
typedef panzer::BlockedTpetraLinearObjContainer<ScalarT, LocalOrdinalT, GlobalOrdinalT> BlockedTpetraLinObjContainerType;
typedef panzer::TpetraLinearObjContainer<ScalarT, LocalOrdinalT, GlobalOrdinalT> TpetraLinObjContainerType;

namespace panzer
{

  Teuchos::RCP<panzer::PureBasis> buildBasis(std::size_t worksetSize, const std::string &basisName);
  void testInitialization(const Teuchos::RCP<Teuchos::ParameterList> &ipb);
  Teuchos::RCP<panzer_stk::STK_Interface> buildMesh(int elemX, int elemY);
  void testGatherScatter(const bool enable_tangents, Teuchos::FancyOStream &out, bool &success);

  // Test without tangent fields in gather evaluator
  TEUCHOS_UNIT_TEST(tpetra_assembly, gather_solution_no_tangents)
  {
    testGatherScatter(false, out, success);
  }

  // Test with tangent fields in gather evaluator
  TEUCHOS_UNIT_TEST(tpetra_assembly, gather_solution_tangents)
  {
    testGatherScatter(true, out, success);
  }

  // enable_tangents determines whether tangent fields dx/dp are added to gather evaluator.
  // These are used when computing df/dx*dx/dp with the tangent evaluation type
  void testGatherScatter(const bool enable_tangents, Teuchos::FancyOStream &out, bool &success)
  {
#ifdef HAVE_MPI
    Teuchos::RCP<const Teuchos::MpiComm<int>> tComm = Teuchos::rcp(new Teuchos::MpiComm<int>(MPI_COMM_WORLD));
#else
    Teuchos::RCP<const Teuchos::SerialComm<int>> tComm = Teuchos::rcp(new Teuchos::SerialComm<int>(MPI_COMM_WORLD));
#endif

    int myRank = tComm->getRank();
    int numProcs = tComm->getSize();

    const std::size_t workset_size = 4 / numProcs;
    const std::string fieldName1_q1 = "U";
    const std::string fieldName2_q1 = "V";
    const std::string fieldName_qedge1 = "B";
    const int num_tangent = enable_tangents ? 5 : 0;

    Teuchos::RCP<panzer_stk::STK_Interface> mesh = buildMesh(2, 2);

    // build input physics block
    Teuchos::RCP<panzer::PureBasis> basis_q1 = buildBasis(workset_size, "Q1");
    Teuchos::RCP<panzer::PureBasis> basis_qedge1 = buildBasis(workset_size, "QEdge1");

    Teuchos::RCP<Teuchos::ParameterList> ipb = Teuchos::parameterList();
    testInitialization(ipb);

    const int default_int_order = 1;
    std::string eBlockID = "eblock-0_0";
    Teuchos::RCP<user_app::MyFactory> eqset_factory = Teuchos::rcp(new user_app::MyFactory);
    panzer::CellData cellData(workset_size, mesh->getCellTopology("eblock-0_0"));
    Teuchos::RCP<panzer::GlobalData> gd = panzer::createGlobalData();
    Teuchos::RCP<panzer::PhysicsBlock> physicsBlock =
        Teuchos::rcp(new PhysicsBlock(ipb, eBlockID, default_int_order, cellData, eqset_factory, gd, false));

    Teuchos::RCP<std::vector<panzer::Workset>> work_sets = panzer_stk::buildWorksets(*mesh, physicsBlock->elementBlockID(),
                                                                                     physicsBlock->getWorksetNeeds());
    TEST_EQUALITY(work_sets->size(), 1);

    // build connection manager and field manager
    const Teuchos::RCP<panzer::ConnManager> conn_manager = Teuchos::rcp(new panzer_stk::STKConnManager(mesh));
    Teuchos::RCP<panzer::BlockedDOFManager> blocked_dofManager = Teuchos::rcp(new panzer::BlockedDOFManager(conn_manager, MPI_COMM_WORLD));

    blocked_dofManager->addField(fieldName1_q1, Teuchos::rcp(new panzer::Intrepid2FieldPattern(basis_q1->getIntrepid2Basis())));
    blocked_dofManager->addField(fieldName2_q1, Teuchos::rcp(new panzer::Intrepid2FieldPattern(basis_q1->getIntrepid2Basis())));
    blocked_dofManager->addField(fieldName_qedge1, Teuchos::rcp(new panzer::Intrepid2FieldPattern(basis_qedge1->getIntrepid2Basis())));

    std::vector<std::vector<std::string> > fieldOrder(3);
    fieldOrder[0].push_back(fieldName1_q1);
    fieldOrder[1].push_back(fieldName_qedge1);
    fieldOrder[2].push_back(fieldName2_q1);
    blocked_dofManager->setFieldOrder(fieldOrder);

    blocked_dofManager->buildGlobalUnknowns();

    // setup linear object factory
    /////////////////////////////////////////////////////////////

    Teuchos::RCP<BlockedTpetraLinObjFactoryType> t_lof = Teuchos::rcp(new BlockedTpetraLinObjFactoryType(tComm.getConst(), blocked_dofManager));
    Teuchos::RCP<LinearObjFactory<panzer::Traits>> lof = t_lof;
    Teuchos::RCP<LinearObjContainer> loc = t_lof->buildGhostedLinearObjContainer();
    t_lof->initializeGhostedContainer(LinearObjContainer::X, *loc);
    loc->initialize();

    Teuchos::RCP<BlockedTpetraLinObjContainerType> t_loc = Teuchos::rcp_dynamic_cast<BlockedTpetraLinObjContainerType>(loc);
    Teuchos::RCP<Thyra::VectorBase<double>> x_vec = t_loc->get_x_th();
    Thyra::assign(x_vec.ptr(), 123.0 + myRank);

    // need a place to evaluate the tangent fields, so we create a 
    // unblocked DOFManager and LOF and set up if needed
    std::vector<Teuchos::RCP<GlobalEvaluationData>> tangentContainers;
    Teuchos::RCP<panzer::DOFManager> dofManager = Teuchos::rcp(new panzer::DOFManager(conn_manager, MPI_COMM_WORLD));
    Teuchos::RCP<TpetraLinObjFactoryType> tangent_lof = Teuchos::rcp(new TpetraLinObjFactoryType(tComm.getConst(), dofManager));
    Teuchos::RCP<LinearObjFactory<panzer::Traits>> parent_tangent_lof = tangent_lof;
 
    if (enable_tangents)
    {
      using Teuchos::RCP;
      using Teuchos::rcp;
      using Teuchos::rcp_dynamic_cast;
      using Thyra::ProductVectorBase;
      using LOCPair = panzer::LOCPair_GlobalEvaluationData;

      std::vector<std::string> tangent_fieldOrder;
      for (int i(0); i < num_tangent; ++i) {
        std::stringstream ssedge;
        ssedge << fieldName_qedge1 << " Tangent " << i;
        std::stringstream ss1, ss2;
        ss1 << fieldName1_q1 << " Tangent " << i;
        ss2 << fieldName2_q1 << " Tangent " << i;
 
        dofManager->addField(ss1.str(), Teuchos::rcp(new panzer::Intrepid2FieldPattern(basis_q1->getIntrepid2Basis())));
        dofManager->addField(ss2.str(), Teuchos::rcp(new panzer::Intrepid2FieldPattern(basis_q1->getIntrepid2Basis())));
        dofManager->addField(ssedge.str(), Teuchos::rcp(new panzer::Intrepid2FieldPattern(basis_qedge1->getIntrepid2Basis())));
        tangent_fieldOrder.push_back(ss1.str());
        tangent_fieldOrder.push_back(ss2.str());
        tangent_fieldOrder.push_back(ssedge.str());
      }
      dofManager->setFieldOrder(tangent_fieldOrder);
      dofManager->buildGlobalUnknowns();

      // generate and evaluate some fields
      Teuchos::RCP<LinearObjContainer> tangent_loc = tangent_lof->buildGhostedLinearObjContainer();
      tangent_lof->initializeGhostedContainer(LinearObjContainer::X, *tangent_loc);
      tangent_loc->initialize();

      for (int i(0); i < num_tangent; ++i)
      {
        auto locPair = Teuchos::rcp(new LOCPair(tangent_lof, panzer::LinearObjContainer::X));

        auto global_t_loc = rcp_dynamic_cast<TpetraLinObjContainerType>(locPair->getGlobalLOC());
        Teuchos::RCP<Thyra::VectorBase<double>> global_x_vec = global_t_loc->get_x_th();
        Thyra::assign(global_x_vec.ptr(), 0.123 + myRank + i);

        auto ghosted_t_loc = rcp_dynamic_cast<TpetraLinObjContainerType>(locPair->getGhostedLOC());
        Teuchos::RCP<Thyra::VectorBase<double>> ghosted_x_vec = ghosted_t_loc->get_x_th();
        Thyra::assign(ghosted_x_vec.ptr(), 0.123 + myRank + i);

        tangentContainers.push_back(locPair);
      } // end loop over the tangents
    }   // end if (enable_tangents)

    // setup field manager, add evaluator under test
    /////////////////////////////////////////////////////////////

    PHX::FieldManager<panzer::Traits> fm;

    std::vector<PHX::index_size_type> derivative_dimensions;
    derivative_dimensions.push_back(12);
    fm.setKokkosExtendedDataTypeDimensions<panzer::Traits::Jacobian>(derivative_dimensions);

    std::vector<PHX::index_size_type> tan_derivative_dimensions;
    if (enable_tangents)
      tan_derivative_dimensions.push_back(num_tangent);
    else
      tan_derivative_dimensions.push_back(0);
    fm.setKokkosExtendedDataTypeDimensions<panzer::Traits::Tangent>(tan_derivative_dimensions);

    Teuchos::RCP<PHX::FieldTag> evalField_q1, evalField_qedge1;
    {
      using Teuchos::RCP;
      using Teuchos::rcp;
      RCP<std::vector<std::string>> names = rcp(new std::vector<std::string>);
      names->push_back(fieldName1_q1);
      names->push_back(fieldName2_q1);

      Teuchos::ParameterList pl;
      pl.set("Basis", basis_q1);
      pl.set("DOF Names", names);
      pl.set("Indexer Names", names);

      Teuchos::RCP<PHX::Evaluator<panzer::Traits>> evaluator = lof->buildGather<panzer::Traits::Residual>(pl);

      TEST_EQUALITY(evaluator->evaluatedFields().size(), 2);

      fm.registerEvaluator<panzer::Traits::Residual>(evaluator);
      fm.requireField<panzer::Traits::Residual>(*evaluator->evaluatedFields()[0]);
    }
    {
      using Teuchos::RCP;
      using Teuchos::rcp;
      RCP<std::vector<std::string>> names = rcp(new std::vector<std::string>);
      names->push_back(fieldName_qedge1);

      Teuchos::ParameterList pl;
      pl.set("Basis", basis_qedge1);
      pl.set("DOF Names", names);
      pl.set("Indexer Names", names);

      Teuchos::RCP<PHX::Evaluator<panzer::Traits>> evaluator = lof->buildGather<panzer::Traits::Residual>(pl);

      TEST_EQUALITY(evaluator->evaluatedFields().size(), 1);

      fm.registerEvaluator<panzer::Traits::Residual>(evaluator);
      fm.requireField<panzer::Traits::Residual>(*evaluator->evaluatedFields()[0]);
    }

    {
      using Teuchos::RCP;
      using Teuchos::rcp;
      RCP<std::vector<std::string>> names = rcp(new std::vector<std::string>);
      names->push_back(fieldName1_q1);
      names->push_back(fieldName2_q1);

      Teuchos::ParameterList pl;
      pl.set("Basis", basis_q1);
      pl.set("DOF Names", names);
      pl.set("Indexer Names", names);

      Teuchos::RCP<PHX::Evaluator<panzer::Traits>> evaluator = lof->buildGather<panzer::Traits::Jacobian>(pl);

      TEST_EQUALITY(evaluator->evaluatedFields().size(), 2);

      fm.registerEvaluator<panzer::Traits::Jacobian>(evaluator);
      fm.requireField<panzer::Traits::Jacobian>(*evaluator->evaluatedFields()[0]);
    }
    {
      using Teuchos::RCP;
      using Teuchos::rcp;
      RCP<std::vector<std::string>> names = rcp(new std::vector<std::string>);
      names->push_back(fieldName_qedge1);

      Teuchos::ParameterList pl;
      pl.set("Basis", basis_qedge1);
      pl.set("DOF Names", names);
      pl.set("Indexer Names", names);

      Teuchos::RCP<PHX::Evaluator<panzer::Traits>> evaluator = lof->buildGather<panzer::Traits::Jacobian>(pl);

      TEST_EQUALITY(evaluator->evaluatedFields().size(), 1);

      fm.registerEvaluator<panzer::Traits::Jacobian>(evaluator);
      fm.requireField<panzer::Traits::Jacobian>(*evaluator->evaluatedFields()[0]);
    }

    {
      using Teuchos::RCP;
      using Teuchos::rcp;
      RCP<std::vector<std::string>> names = rcp(new std::vector<std::string>);
      names->push_back(fieldName1_q1);
      names->push_back(fieldName2_q1);

      Teuchos::ParameterList pl;
      pl.set("Basis", basis_q1);
      pl.set("DOF Names", names);
      pl.set("Indexer Names", names);

      if (enable_tangents)
      {
        RCP<std::vector<std::vector<std::string>>> tangent_names =
            rcp(new std::vector<std::vector<std::string>>(2));
        for (int i = 0; i < num_tangent; ++i)
        {
          std::stringstream ss1, ss2;
          ss1 << fieldName1_q1 << " Tangent " << i;
          ss2 << fieldName2_q1 << " Tangent " << i;
          (*tangent_names)[0].push_back(ss1.str());
          (*tangent_names)[1].push_back(ss2.str());
        }
        pl.set("Tangent Names", tangent_names);
      }

      Teuchos::RCP<PHX::Evaluator<panzer::Traits>> evaluator = lof->buildGather<panzer::Traits::Tangent>(pl);

      TEST_EQUALITY(evaluator->evaluatedFields().size(), 2);

      fm.registerEvaluator<panzer::Traits::Tangent>(evaluator);
      fm.requireField<panzer::Traits::Tangent>(*evaluator->evaluatedFields()[0]);
    }
    {
      using Teuchos::RCP;
      using Teuchos::rcp;
      RCP<std::vector<std::string>> names = rcp(new std::vector<std::string>);
      names->push_back(fieldName_qedge1);

      Teuchos::ParameterList pl;
      pl.set("Basis", basis_qedge1);
      pl.set("DOF Names", names);
      pl.set("Indexer Names", names);

      if (enable_tangents)
      {
        RCP<std::vector<std::vector<std::string>>> tangent_names =
            rcp(new std::vector<std::vector<std::string>>(1));
        for (int i = 0; i < num_tangent; ++i)
        {
          std::stringstream ss;
          ss << fieldName_qedge1 << " Tangent " << i;
          (*tangent_names)[0].push_back(ss.str());
        }
        pl.set("Tangent Names", tangent_names);
      }

      Teuchos::RCP<PHX::Evaluator<panzer::Traits>> evaluator = lof->buildGather<panzer::Traits::Tangent>(pl);

      TEST_EQUALITY(evaluator->evaluatedFields().size(), 1);

      fm.registerEvaluator<panzer::Traits::Tangent>(evaluator);
      fm.requireField<panzer::Traits::Tangent>(*evaluator->evaluatedFields()[0]);
    }

    if (enable_tangents)
    {
      for (int i = 0; i < num_tangent; ++i)
      {
        using Teuchos::RCP;
        using Teuchos::rcp;
        RCP<std::vector<std::string>> names = rcp(new std::vector<std::string>);
        RCP<std::vector<std::string>> tangent_names = rcp(new std::vector<std::string>);
        names->push_back(fieldName1_q1);
        names->push_back(fieldName2_q1);
        {
          std::stringstream ss1, ss2;
          ss1 << fieldName1_q1 << " Tangent " << i;
          ss2 << fieldName2_q1 << " Tangent " << i;
          tangent_names->push_back(ss1.str());
          tangent_names->push_back(ss2.str());
        }

        Teuchos::ParameterList pl;
        pl.set("Basis", basis_q1);
        pl.set("DOF Names", tangent_names);
        pl.set("Indexer Names", tangent_names);

        {
          std::stringstream ss;
          ss << "Tangent Container " << i;
          pl.set("Global Data Key", ss.str());
        }

        Teuchos::RCP<PHX::Evaluator<panzer::Traits>> evaluator =
            parent_tangent_lof->buildGatherTangent<panzer::Traits::Tangent>(pl);

        TEST_EQUALITY(evaluator->evaluatedFields().size(), 2);

        fm.registerEvaluator<panzer::Traits::Tangent>(evaluator);
      }
      for (int i = 0; i < num_tangent; ++i)
      {
        using Teuchos::RCP;
        using Teuchos::rcp;
        RCP<std::vector<std::string>> names = rcp(new std::vector<std::string>);
        RCP<std::vector<std::string>> tangent_names = rcp(new std::vector<std::string>);
        names->push_back(fieldName_qedge1);
        {
          std::stringstream ss;
          ss << fieldName_qedge1 << " Tangent " << i;
          tangent_names->push_back(ss.str());
        }

        Teuchos::ParameterList pl;
        pl.set("Basis", basis_qedge1);
        pl.set("DOF Names", tangent_names);
        pl.set("Indexer Names", tangent_names);

        {
          std::stringstream ss;
          ss << "Tangent Container " << i;
          pl.set("Global Data Key", ss.str());
        }

        Teuchos::RCP<PHX::Evaluator<panzer::Traits>> evaluator =
            parent_tangent_lof->buildGatherTangent<panzer::Traits::Tangent>(pl);

        TEST_EQUALITY(evaluator->evaluatedFields().size(), 1);

        fm.registerEvaluator<panzer::Traits::Tangent>(evaluator);
      }
    }

    panzer::Traits::SD sd;

    panzer::Workset &workset = (*work_sets)[0];
    workset.alpha = 0.0;
    workset.beta = 2.0; // derivatives multiplied by 2
    workset.time = 0.0;
    workset.evaluate_transient_terms = false;

    sd.worksets_ = work_sets;

    fm.postRegistrationSetup(sd);

    panzer::Traits::PED ped;
    ped.gedc->addDataObject("Solution Gather Container", loc);
    if (enable_tangents)
    {
      for (int i(0); i < num_tangent; ++i)
      {
        std::stringstream ss;
        ss << "Tangent Container " << i;
        ped.gedc->addDataObject(ss.str(), tangentContainers[i]);
      }
    }

    fm.preEvaluate<panzer::Traits::Residual>(ped);
    fm.evaluateFields<panzer::Traits::Residual>(workset);
    fm.postEvaluate<panzer::Traits::Residual>(0);

    fm.preEvaluate<panzer::Traits::Jacobian>(ped);
    fm.evaluateFields<panzer::Traits::Jacobian>(workset);
    fm.postEvaluate<panzer::Traits::Jacobian>(0);

    fm.preEvaluate<panzer::Traits::Tangent>(ped);
    fm.evaluateFields<panzer::Traits::Tangent>(workset);
    fm.postEvaluate<panzer::Traits::Tangent>(0);

    // test Residual fields
    {
      PHX::MDField<panzer::Traits::Residual::ScalarT, panzer::Cell, panzer::BASIS>
          fieldData1_q1(fieldName1_q1, basis_q1->functional);
      PHX::MDField<panzer::Traits::Residual::ScalarT, panzer::Cell, panzer::BASIS>
          fieldData2_q1(fieldName2_q1, basis_qedge1->functional);

      fm.getFieldData<panzer::Traits::Residual>(fieldData1_q1);
      fm.getFieldData<panzer::Traits::Residual>(fieldData2_q1);

      TEST_EQUALITY(fieldData1_q1.extent(0), Teuchos::as<unsigned int>(4 / numProcs));
      TEST_EQUALITY(fieldData1_q1.extent(1), 4);
      TEST_EQUALITY(fieldData2_q1.extent(0), Teuchos::as<unsigned int>(4 / numProcs));
      TEST_EQUALITY(fieldData2_q1.extent(1), 4);
      TEST_EQUALITY(fieldData1_q1.size(), Teuchos::as<unsigned int>(4 * 4 / numProcs));
      TEST_EQUALITY(fieldData2_q1.size(), Teuchos::as<unsigned int>(4 * 4 / numProcs));

      auto fieldData1_q1_h = Kokkos::create_mirror_view(fieldData1_q1.get_static_view());
      auto fieldData2_q1_h = Kokkos::create_mirror_view(fieldData2_q1.get_static_view());
      Kokkos::deep_copy(fieldData1_q1_h, fieldData1_q1.get_static_view());
      Kokkos::deep_copy(fieldData2_q1_h, fieldData2_q1.get_static_view());

      for (unsigned int i = 0; i < fieldData1_q1.extent(0); i++)
        for (unsigned int j = 0; j < fieldData1_q1.extent(1); j++)
          TEST_EQUALITY(fieldData1_q1_h(i, j), 123.0 + myRank);

      for (unsigned int i = 0; i < fieldData2_q1.extent(0); i++)
        for (unsigned int j = 0; j < fieldData2_q1.extent(1); j++)
          TEST_EQUALITY(fieldData2_q1_h(i, j), 123.0 + myRank);
    }
    {
      PHX::MDField<panzer::Traits::Residual::ScalarT, panzer::Cell, panzer::BASIS>
          fieldData_qedge1(fieldName_qedge1, basis_qedge1->functional);

      fm.getFieldData<panzer::Traits::Residual>(fieldData_qedge1);

      auto fieldData_qedge1_h = Kokkos::create_mirror_view(fieldData_qedge1.get_static_view());
      Kokkos::deep_copy(fieldData_qedge1_h, fieldData_qedge1.get_static_view());

      TEST_EQUALITY(fieldData_qedge1.extent(0), Teuchos::as<unsigned int>(4 / numProcs));
      TEST_EQUALITY(fieldData_qedge1.extent(1), 4);
      TEST_EQUALITY(fieldData_qedge1.size(), Teuchos::as<unsigned int>(4 * 4 / numProcs));

      for (unsigned int cell = 0; cell < fieldData_qedge1.extent(0); ++cell)
        for (unsigned int pt = 0; pt < fieldData_qedge1.extent(1); pt++)
          TEST_EQUALITY(fieldData_qedge1_h(cell, pt), 123.0 + myRank);
    }

    // test Jacobian fields
    {
      PHX::MDField<panzer::Traits::Jacobian::ScalarT, panzer::Cell, panzer::BASIS>
          fieldData1_q1(fieldName1_q1, basis_q1->functional);
      PHX::MDField<panzer::Traits::Jacobian::ScalarT, panzer::Cell, panzer::BASIS>
          fieldData2_q1(fieldName2_q1, basis_qedge1->functional);

      fm.getFieldData<panzer::Traits::Jacobian>(fieldData1_q1);
      fm.getFieldData<panzer::Traits::Jacobian>(fieldData2_q1);

      auto fieldData1_q1_h = Kokkos::create_mirror_view(fieldData1_q1.get_static_view());
      auto fieldData2_q1_h = Kokkos::create_mirror_view(fieldData2_q1.get_static_view());
      Kokkos::deep_copy(fieldData1_q1_h, fieldData1_q1.get_static_view());
      Kokkos::deep_copy(fieldData2_q1_h, fieldData2_q1.get_static_view());

      for (unsigned int cell = 0; cell < fieldData1_q1.extent(0); ++cell)
      {
        for (unsigned int pt = 0; pt < fieldData1_q1.extent(1); pt++)
        {
          TEST_EQUALITY(fieldData1_q1_h(cell, pt), 123.0 + myRank);
          TEST_EQUALITY(fieldData1_q1_h(cell, pt).availableSize(), 12);
        }
      }
      for (unsigned int cell = 0; cell < fieldData2_q1.extent(0); ++cell)
      {
        for (unsigned int pt = 0; pt < fieldData2_q1.extent(1); pt++)
        {
          TEST_EQUALITY(fieldData2_q1_h(cell, pt), 123.0 + myRank);
          TEST_EQUALITY(fieldData2_q1_h(cell, pt).availableSize(), 12);
        }
      }
    }
    {
      PHX::MDField<panzer::Traits::Jacobian::ScalarT, panzer::Cell, panzer::BASIS>
          fieldData_qedge1(fieldName_qedge1, basis_qedge1->functional);

      fm.getFieldData<panzer::Traits::Jacobian>(fieldData_qedge1);

      auto fieldData_qedge1_h = Kokkos::create_mirror_view(fieldData_qedge1.get_static_view());
      Kokkos::deep_copy(fieldData_qedge1_h, fieldData_qedge1.get_static_view());

      for (unsigned int cell = 0; cell < fieldData_qedge1.extent(0); ++cell)
      {
        for (unsigned int pt = 0; pt < fieldData_qedge1.extent(1); ++pt)
        {
          TEST_EQUALITY(fieldData_qedge1_h(cell, pt), 123.0 + myRank);
          TEST_EQUALITY(fieldData_qedge1_h(cell, pt).availableSize(), 12);
        }
      }
    }

    // test Tangent fields
    {
      PHX::MDField<panzer::Traits::Tangent::ScalarT, panzer::Cell, panzer::BASIS>
          fieldData1_q1(fieldName1_q1, basis_q1->functional);
      PHX::MDField<panzer::Traits::Tangent::ScalarT, panzer::Cell, panzer::BASIS>
          fieldData2_q1(fieldName2_q1, basis_qedge1->functional);

      fm.getFieldData<panzer::Traits::Tangent>(fieldData1_q1);
      fm.getFieldData<panzer::Traits::Tangent>(fieldData2_q1);

      auto fieldData1_q1_h = Kokkos::create_mirror_view(fieldData1_q1.get_static_view());
      auto fieldData2_q1_h = Kokkos::create_mirror_view(fieldData2_q1.get_static_view());
      Kokkos::deep_copy(fieldData1_q1_h, fieldData1_q1.get_static_view());
      Kokkos::deep_copy(fieldData2_q1_h, fieldData2_q1.get_static_view());

      for (unsigned int cell = 0; cell < fieldData1_q1.extent(0); ++cell)
      {
        for (unsigned int pt = 0; pt < fieldData1_q1.extent(1); pt++)
        {
          if (enable_tangents)
          {
            TEST_EQUALITY(fieldData1_q1_h(cell, pt).val(), 123.0 + myRank);
            TEST_EQUALITY(fieldData1_q1_h(cell, pt).availableSize(), num_tangent);
            for (int i = 0; i < num_tangent; ++i)
              TEST_EQUALITY(fieldData1_q1_h(cell, pt).dx(i), 0.123 + myRank + i);
          }
          else
          {
            TEST_EQUALITY(fieldData1_q1_h(cell, pt), 123.0 + myRank);
            TEST_EQUALITY(fieldData1_q1_h(cell, pt).availableSize(), 0);
          }
        }
      }
      for (unsigned int cell = 0; cell < fieldData2_q1.extent(0); ++cell)
      {
        for (unsigned int pt = 0; pt < fieldData2_q1.extent(1); pt++)
        {
          if (enable_tangents)
          {
            TEST_EQUALITY(fieldData2_q1_h(cell, pt).val(), 123.0 + myRank);
            TEST_EQUALITY(fieldData2_q1_h(cell, pt).availableSize(), num_tangent);
            for (int i = 0; i < num_tangent; ++i)
            {
              TEST_EQUALITY(fieldData2_q1_h(cell, pt).dx(i), 0.123 + myRank + i);
              TEST_EQUALITY(fieldData2_q1_h(cell, pt).dx(i), 0.123 + myRank + i);
            }
          }
          else
          {
            TEST_EQUALITY(fieldData2_q1_h(cell, pt), 123.0 + myRank);
            TEST_EQUALITY(fieldData2_q1_h(cell, pt).availableSize(), 0);
          }
        }
      }
    }
    {
      PHX::MDField<panzer::Traits::Tangent::ScalarT, panzer::Cell, panzer::BASIS>
          fieldData_qedge1(fieldName_qedge1, basis_qedge1->functional);

      fm.getFieldData<panzer::Traits::Tangent>(fieldData_qedge1);

      auto fieldData_qedge1_h = Kokkos::create_mirror_view(fieldData_qedge1.get_static_view());
      Kokkos::deep_copy(fieldData_qedge1_h, fieldData_qedge1.get_static_view());

      for (unsigned int cell = 0; cell < fieldData_qedge1.extent(0); ++cell)
      {
        for (unsigned int pt = 0; pt < fieldData_qedge1.extent(1); ++pt)
        {
          if (enable_tangents)
          {
            TEST_EQUALITY(fieldData_qedge1_h(cell, pt).val(), 123.0 + myRank);
            TEST_EQUALITY(fieldData_qedge1_h(cell, pt).availableSize(), num_tangent);
            for (int i = 0; i < num_tangent; ++i)
              TEST_EQUALITY(fieldData_qedge1_h(cell, pt).dx(i), 0.123 + myRank + i);
          }
          else
          {
            TEST_EQUALITY(fieldData_qedge1_h(cell, pt), 123.0 + myRank);
            TEST_EQUALITY(fieldData_qedge1_h(cell, pt).availableSize(), 0);
          }
        }
      }
    }
  }

  Teuchos::RCP<panzer::PureBasis> buildBasis(std::size_t worksetSize, const std::string &basisName)
  {
    Teuchos::RCP<shards::CellTopology> topo =
        Teuchos::rcp(new shards::CellTopology(shards::getCellTopologyData<shards::Quadrilateral<4>>()));

    panzer::CellData cellData(worksetSize, topo);
    return Teuchos::rcp(new panzer::PureBasis(basisName, 1, cellData));
  }

  Teuchos::RCP<panzer_stk::STK_Interface> buildMesh(int elemX, int elemY)
  {
    Teuchos::RCP<Teuchos::ParameterList> pl = rcp(new Teuchos::ParameterList);
    pl->set("X Blocks", 1);
    pl->set("Y Blocks", 1);
    pl->set("X Elements", elemX);
    pl->set("Y Elements", elemY);

    panzer_stk::SquareQuadMeshFactory factory;
    factory.setParameterList(pl);
    Teuchos::RCP<panzer_stk::STK_Interface> mesh = factory.buildUncommitedMesh(MPI_COMM_WORLD);
    factory.completeMeshConstruction(*mesh, MPI_COMM_WORLD);

    return mesh;
  }

  void testInitialization(const Teuchos::RCP<Teuchos::ParameterList> &ipb)
  {
    // Physics block
    ipb->setName("test physics");
    {
      Teuchos::ParameterList &p = ipb->sublist("a");
      p.set("Type", "Energy");
      p.set("Prefix", "");
      p.set("Model ID", "solid");
      p.set("Basis Type", "HGrad");
      p.set("Basis Order", 1);
      p.set("Integration Order", 1);
    }
    {
      Teuchos::ParameterList &p = ipb->sublist("b");
      p.set("Type", "Energy");
      p.set("Prefix", "ION_");
      p.set("Model ID", "solid");
      p.set("Basis Type", "HCurl");
      p.set("Basis Order", 1);
      p.set("Integration Order", 1);
    }
  }

}
